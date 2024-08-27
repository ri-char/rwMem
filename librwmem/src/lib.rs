use bitvec::{order::Lsb0, vec::BitVec};
use byteorder::{NativeEndian, ReadBytesExt};
use nix::{ioctl_none, ioctl_write_int, ioctl_write_ptr, request_code_readwrite};
use std::{
    cmp::max,
    io::{Cursor, Read},
    os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd},
    path::Path,
};

pub mod errors;

type Result<T> = std::result::Result<T, errors::Error>;

const RWMEM_MAGIC: u8 = 100;
const RWMEM_BP_MAGIC: u8 = 101;
const IOCTL_GET_PROCESS_MAPS_COUNT: u8 = 0;
const IOCTL_GET_PROCESS_MAPS_LIST: u8 = 1;
const IOCTL_CHECK_PROCESS_ADDR_PHY: u8 = 2;

#[derive(Debug, PartialEq, Eq)]
pub struct MapsEntry {
    pub start: u64,
    pub end: u64,
    pub read_permission: bool,
    pub write_permission: bool,
    pub execute_permission: bool,
    pub shared: bool,
    pub name: String,
}

pub const DEFAULT_DRIVER_PATH: &str = "/dev/rwmem";

#[repr(transparent)]
#[derive(Debug)]
pub struct Device {
    fd: OwnedFd,
}

#[allow(dead_code)]
impl Device {
    /// Create a new device. The default path is `DEFAULT_DRIVER_PATH`.
    pub fn new<P: AsRef<Path>>(path: P) -> Result<Self> {
        let fd = nix::fcntl::open(
            path.as_ref(),
            nix::fcntl::OFlag::O_RDWR,
            nix::sys::stat::Mode::empty(),
        )?;
        Ok(Self {
            fd: unsafe { OwnedFd::from_raw_fd(fd) },
        })
    }

    pub fn from_raw_fd(fd: RawFd) -> Self {
        Self {
            fd: unsafe { OwnedFd::from_raw_fd(fd) },
        }
    }
    pub fn as_raw_fd(&self) -> RawFd {
        self.fd.as_raw_fd()
    }
    /// read the memory of a process.
    pub fn read_mem(&self, pid: i32, addr: u64, buf: &mut [u8]) -> Result<()> {
        if buf.len() < 17 {
            let new_buf = &mut [0u8; 17];
            new_buf[0..8].copy_from_slice(&(pid as i64).to_ne_bytes());
            new_buf[8..16].copy_from_slice(&addr.to_ne_bytes());
            let real_read = nix::errno::Errno::result(unsafe {
                libc::read(
                    self.fd.as_raw_fd(),
                    new_buf.as_mut_ptr() as *mut libc::c_void,
                    buf.len(),
                )
            })?;
            if real_read != buf.len() as isize {
                return Err(errors::Error::ReadFailed(buf.len(), real_read as usize));
            }
            buf.copy_from_slice(&new_buf[..buf.len()]);
        } else {
            buf[0..8].copy_from_slice(&(pid as i64).to_ne_bytes());
            buf[8..16].copy_from_slice(&addr.to_ne_bytes());
            buf[17] = 0;
            let real_read = nix::errno::Errno::result(unsafe {
                libc::read(
                    self.fd.as_raw_fd(),
                    buf.as_mut_ptr() as *mut libc::c_void,
                    buf.len(),
                )
            })?;
            if real_read != buf.len() as isize {
                return Err(errors::Error::ReadFailed(buf.len(), real_read as usize));
            }
        }
        Ok(())
    }

    /// write the memory of a process.
    pub fn write_mem(&self, pid: i32, addr: u64, buf: &[u8]) -> Result<()> {
        let mut new_buf = vec![0u8; 17 + buf.len()];
        new_buf[0..8].copy_from_slice(&(pid as i64).to_ne_bytes());
        new_buf[8..16].copy_from_slice(&addr.to_ne_bytes());
        new_buf[16] = 1;
        new_buf[17..].copy_from_slice(buf);
        let real_write = nix::errno::Errno::result(unsafe {
            libc::write(
                self.fd.as_raw_fd(),
                new_buf.as_ptr() as *const libc::c_void,
                buf.len(),
            )
        })?;
        if real_write != buf.len() as isize {
            return Err(errors::Error::WriteFailed(buf.len(), real_write as usize));
        }
        Ok(())
    }

    /// get the memory map of a process.
    pub fn get_mem_map(&self, pid: i32, phy_only: bool) -> Result<Vec<MapsEntry>> {
        let count = self.get_mem_map_count(pid)?;
        let buf_len = 8 + (8 + 8 + 4 + 512) * (count + 50);
        let mut buf = vec![0u8; buf_len];
        buf[0..8].copy_from_slice(&(pid as i64).to_ne_bytes());
        buf[8..16].copy_from_slice(&512usize.to_ne_bytes());
        buf[16..24].copy_from_slice(&buf_len.to_ne_bytes());

        let unfinished = nix::errno::Errno::result(unsafe {
            libc::ioctl(
                self.fd.as_raw_fd(),
                request_code_readwrite!(
                    RWMEM_MAGIC,
                    IOCTL_GET_PROCESS_MAPS_LIST,
                    std::mem::size_of::<usize>()
                ),
                buf.as_mut_ptr(),
                buf.len(),
            )
        })?;
        if unfinished != 0 {
            return Err(errors::Error::MapsTooLong);
        }
        let mut cursor = Cursor::new(buf);
        let real_count = cursor.read_u64::<NativeEndian>()?;
        let mut result = Vec::with_capacity(real_count as usize);
        for _ in 0..real_count {
            let start = cursor.read_u64::<NativeEndian>()?;
            let end = cursor.read_u64::<NativeEndian>()?;
            let read_permission = cursor.read_u8()? != 0;
            let write_permission = cursor.read_u8()? != 0;
            let execute_permission = cursor.read_u8()? != 0;
            let shared = cursor.read_u8()? != 0;
            let mut name = [0u8; 512];
            cursor.read_exact(&mut name)?;
            let mut name_length = 512usize;
            for (i, v) in name.iter().enumerate() {
                if *v == 0 {
                    name_length = i;
                    break;
                }
            }
            let name = String::from_utf8_lossy(&name[0..name_length]).to_string();
            if phy_only {
                let is_mem_phy = self.is_mem_phy(pid, start, end)?;
                let mut is_last_phy = false;
                let mut begin_phy = 0u64;
                for (i, is_phy) in is_mem_phy.iter().enumerate() {
                    if *is_phy {
                        if !is_last_phy {
                            begin_phy = start + i as u64 * 0x1000;
                            is_last_phy = true;
                        }
                    } else if is_last_phy {
                        let entry = MapsEntry {
                            start: begin_phy,
                            end: start + i as u64 * 0x1000,
                            read_permission,
                            write_permission,
                            execute_permission,
                            shared,
                            name: name.clone(),
                        };
                        is_last_phy = false;
                        result.push(entry);
                    }
                }
                if is_last_phy {
                    let entry = MapsEntry {
                        start: begin_phy,
                        end,
                        read_permission,
                        write_permission,
                        execute_permission,
                        shared,
                        name,
                    };
                    result.push(entry);
                }
            } else {
                let entry = MapsEntry {
                    start,
                    end,
                    read_permission,
                    write_permission,
                    execute_permission,
                    shared,
                    name,
                };
                result.push(entry);
            }
        }
        Ok(result)
    }

    /// get the count of memory map entries.
    fn get_mem_map_count(&self, pid: i32) -> Result<usize> {
        ioctl_write_int!(get_mem_map_count, RWMEM_MAGIC, IOCTL_GET_PROCESS_MAPS_COUNT);
        let count = unsafe { get_mem_map_count(self.fd.as_raw_fd(), pid as u64) }?;
        Ok(count as usize)
    }

    /// check if the memory is physical.
    /// begin_addr and end_addr must be page aligned.
    /// return a bitvec, each bit represents a page.
    pub fn is_mem_phy(&self, pid: i32, begin_addr: u64, end_addr: u64) -> Result<BitVec<u8, Lsb0>> {
        if begin_addr & 0xfff != 0 {
            return Err(errors::Error::NotAligned);
        }
        if end_addr & 0xfff != 0 {
            return Err(errors::Error::NotAligned);
        }
        if begin_addr >= end_addr {
            return Err(errors::Error::BeginLargerThanEnd(begin_addr, end_addr));
        }
        let buf_size = max(24, ((end_addr >> 12) - (begin_addr >> 12) + 7) / 8) as usize;
        let mut buf = vec![0u8; buf_size];
        buf[0..8].copy_from_slice(&(pid as i64).to_ne_bytes());
        buf[8..16].copy_from_slice(&begin_addr.to_ne_bytes());
        buf[16..24].copy_from_slice(&end_addr.to_ne_bytes());
        nix::errno::Errno::result(unsafe {
            libc::ioctl(
                self.fd.as_raw_fd(),
                request_code_readwrite!(
                    RWMEM_MAGIC,
                    IOCTL_CHECK_PROCESS_ADDR_PHY,
                    std::mem::size_of::<usize>()
                ),
                buf.as_mut_ptr(),
                buf_size,
            )
        })?;
        let mut result = BitVec::<u8, Lsb0>::from_vec(buf);
        result.truncate(((end_addr >> 12) - (begin_addr >> 12)) as usize);
        Ok(result)
    }

    /// add bp
    pub fn add_bp(
        &self,
        pid: i32,
        bp_type: BreakpointType,
        len: u8,
        addr: u64,
    ) -> Result<Breakpoint> {
        let mut buf = [0u8; 16];

        buf[0..4].copy_from_slice(&pid.to_ne_bytes());
        buf[4] = match bp_type {
            BreakpointType::Read => 1,
            BreakpointType::Write => 2,
            BreakpointType::ReadWrite => 3,
            BreakpointType::Execute => 4,
        };
        buf[5] = len;
        buf[8..16].copy_from_slice(&addr.to_ne_bytes());
        let fd = nix::errno::Errno::result(unsafe {
            libc::ioctl(
                self.fd.as_raw_fd(),
                request_code_readwrite!(RWMEM_MAGIC, 3, std::mem::size_of::<usize>()),
                buf.as_ptr(),
                16,
            )
        })?;
        Ok(Breakpoint::from_raw_fd(fd))
    }

    pub fn get_num_brps(&self) -> Result<i32> {
        ioctl_none!(get_num_brps, RWMEM_MAGIC, 4);
        let num = unsafe { get_num_brps(self.fd.as_raw_fd()) }?;
        Ok(num)
    }

    pub fn get_num_wrps(&self) -> Result<i32> {
        ioctl_none!(get_num_wrps, RWMEM_MAGIC, 5);
        let num = unsafe { get_num_wrps(self.fd.as_raw_fd()) }?;
        Ok(num)
    }
}

#[derive(Debug)]
pub struct Breakpoint {
    fd: OwnedFd,
}

#[repr(C)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Regs {
    pub regs: [u64; 31],
    pub sp: u64,
    pub pc: u64,
    pub pstate: u64,
}

#[repr(C)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SimdRegs {
    pub regs: [u128; 32],
    pub fpsr: u32,
    pub fpcr: u32,
    __: [u32; 2],
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BreakpointType {
    Read,
    Write,
    ReadWrite,
    Execute,
}

impl std::str::FromStr for BreakpointType {
    type Err = errors::Error;
    fn from_str(s: &str) -> std::result::Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "r" => Ok(Self::Read),
            "w" => Ok(Self::Write),
            "rw" => Ok(Self::ReadWrite),
            "x" => Ok(Self::Execute),
            "read" => Ok(Self::Read),
            "write" => Ok(Self::Write),
            "readwrite" => Ok(Self::ReadWrite),
            "read_write" => Ok(Self::ReadWrite),
            "execute" => Ok(Self::Execute),
            _ => Err(errors::Error::InvalidBreakpointType(s.to_string())),
        }
    }
}

pub type AllRegs = (Regs, SimdRegs);

#[allow(dead_code)]
impl Breakpoint {
    pub fn from_raw_fd(fd: RawFd) -> Self {
        Self {
            fd: unsafe { OwnedFd::from_raw_fd(fd) },
        }
    }

    pub fn as_raw_fd(&self) -> RawFd {
        self.fd.as_raw_fd()
    }

    pub fn cont(&self) -> Result<i32> {
        ioctl_none!(bp_continue, RWMEM_BP_MAGIC, 0);
        let num = unsafe { bp_continue(self.fd.as_raw_fd()) }?;
        Ok(num)
    }

    pub fn set_reg(&self, reg: u8, value: u64) -> Result<()> {
        #[repr(C)]
        struct SetRegParam {
            reg: u64,
            value: u64,
        }
        ioctl_write_ptr!(bp_set_reg, RWMEM_BP_MAGIC, 1, SetRegParam);
        if reg >= 34 {
            return Err(errors::Error::InvalidRegister(reg as u64));
        }
        let param = SetRegParam {
            reg: reg as u64,
            value,
        };
        unsafe { bp_set_reg(self.fd.as_raw_fd(), &param) }?;
        Ok(())
    }

    pub fn set_simd_reg(&self, reg: u8, value: u128) -> Result<()> {
        #[repr(C)]
        struct SetSimdRegParam {
            reg: u64,
            value: u128,
        }
        ioctl_write_ptr!(bp_set_simd_reg, RWMEM_BP_MAGIC, 2, SetSimdRegParam);
        if reg >= 34 {
            return Err(errors::Error::InvalidRegister(reg as u64));
        }
        let param = SetSimdRegParam {
            reg: reg as u64,
            value,
        };
        unsafe { bp_set_simd_reg(self.fd.as_raw_fd(), &param) }?;
        Ok(())
    }

    pub fn get_regs(&self) -> Result<Regs> {
        const BUF_SIZE: usize = std::mem::size_of::<Regs>();
        let mut buf = [0u8; BUF_SIZE];
        nix::unistd::read(self.fd.as_raw_fd(), &mut buf)?;
        let regs = unsafe { std::mem::transmute::<[u8; BUF_SIZE], Regs>(buf) };
        Ok(regs)
    }

    pub fn get_regs_with_simd(&self) -> Result<(Regs, SimdRegs)> {
        const BUF_SIZE: usize = std::mem::size_of::<(Regs, SimdRegs)>();
        let mut buf = [0u8; BUF_SIZE];
        nix::unistd::read(self.fd.as_raw_fd(), &mut buf)?;
        let regs = unsafe { std::mem::transmute::<[u8; BUF_SIZE], (Regs, SimdRegs)>(buf) };
        Ok(regs)
    }

    #[cfg(feature = "async")]
    pub async fn wait(&self) -> Result<()> {
        let async_fd = tokio::io::unix::AsyncFd::with_interest(
            self.as_raw_fd(),
            tokio::io::Interest::READABLE,
        )?;
        async_fd.readable().await?.clear_ready();
        Ok(())
    }

    pub fn step(&self) -> Result<i32> {
        ioctl_none!(bp_step, RWMEM_BP_MAGIC, 3);
        let num = unsafe { bp_step(self.fd.as_raw_fd()) }?;
        Ok(num)
    }

    pub fn is_stopped(&self) -> Result<bool> {
        ioctl_none!(bp_is_stopped, RWMEM_BP_MAGIC, 4);
        let stopped = unsafe { bp_is_stopped(self.fd.as_raw_fd()) }?;
        Ok(stopped != 0)
    }
}
