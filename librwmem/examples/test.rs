use librwmem::{BreakpointType, Device, DEFAULT_DRIVER_PATH};

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let device = Device::new(DEFAULT_DRIVER_PATH)?;
    let pid = 0x1234;
    let addr = 0x12345678;

    // write to remote process memory
    device.write_mem(pid, addr, &[0xcc, 0xcc, 0xcc, 0xcc])?;

    // read from remote process memory
    let mut buf = [0u8; 1024];
    device.read_mem(pid, addr, &mut buf)?;

    // add a breakpoint
    let bp = device.add_bp(pid, BreakpointType::ReadWrite, 8, addr)?;

    // wait for the breakpoint to be hit
    bp.wait().await?;

    // print the registers
    println!("{:x?}", bp.get_regs()?);

    // set the registers, set X8 to 0
    bp.set_reg(8, 0)?;

    // run a single instruction
    bp.step()?;
    bp.wait().await?;

    // continue the process
    bp.cont()?;

    // remove the breakpoint
    std::mem::drop(bp);
    Ok(())
}
