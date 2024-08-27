use std::{collections::HashMap, io::Write};

use clap::{Parser, Subcommand};
use clap_num::maybe_hex;
use librwmem::{Breakpoint, BreakpointType, Device};

#[derive(Parser)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    AddBp {
        pid: i32,
        #[clap(value_parser=clap::value_parser!(BreakpointType))]
        bp_type: BreakpointType,
        len: u8,
        #[clap(value_parser=maybe_hex::<u64>)]
        addr: u64,
    },
    DelBp {
        id: i32,
    },
    Continue {
        id: i32,
    },
    SetReg {
        id: i32,
        #[clap(value_parser=maybe_hex::<u8>)]
        reg_id: u8,
        #[clap(value_parser=maybe_hex::<u64>)]
        value: u64,
    },
    SetSimdReg {
        id: i32,
        #[clap(value_parser=maybe_hex::<u8>)]
        reg_id: u8,
        #[clap(value_parser=maybe_hex::<u128>)]
        value: u128,
    },
    GetRegs {
        id: i32,
        #[clap(short)]
        simd: bool,
    },
    Wait {
        id: i32,
    },
    GetMemMap {
        pid: i32,
    },
    WriteMem {
        pid: i32,
        #[clap(value_parser=maybe_hex::<u64>)]
        addr: u64,
        #[clap(value_parser=clap::value_parser!(MyBytes))]
        value: MyBytes,
    },
    ReadMem {
        pid: i32,
        #[clap(value_parser=maybe_hex::<u64>)]
        addr: u64,
        #[clap(value_parser=maybe_hex::<u64>)]
        size: u64,
    },
    Step {
        id: i32,
    },
    IsStopped {
        id: i32,
    },
}

#[derive(Debug, Clone)]
struct MyBytes(Vec<u8>);
impl std::str::FromStr for MyBytes {
    type Err = String;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        hex::decode(s).map_err(|e| e.to_string()).map(MyBytes)
    }
}

fn run_cmd(
    cmd: Commands,
    device: &Device,
    bps: &mut HashMap<i32, Breakpoint>,
) -> anyhow::Result<()> {
    match cmd {
        Commands::AddBp {
            pid,
            bp_type,
            len,
            addr,
        } => {
            let bp = device.add_bp(pid, bp_type, len, addr)?;
            let id = bp.as_raw_fd();
            bps.insert(id, bp);
            println!("Breakpoint added: {:?}", id);
        }
        Commands::DelBp { id } => {
            let bp = bps
                .remove(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            std::mem::drop(bp);
        }
        Commands::Continue { id } => {
            let bp = bps
                .get(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            bp.cont()?;
        }
        Commands::SetReg { id, reg_id, value } => {
            let bp = bps
                .get(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            bp.set_reg(reg_id, value)?;
        }
        Commands::SetSimdReg { id, reg_id, value } => {
            let bp = bps
                .get(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            bp.set_simd_reg(reg_id, value)?;
        }
        Commands::GetRegs { id, simd } => {
            let bp = bps
                .get(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            if simd {
                let regs = bp.get_regs_with_simd()?;
                println!("Hit breakpoint: {:#?}", regs);
            } else {
                let regs = bp.get_regs()?;
                println!("Hit breakpoint: {:#?}", regs);
            }
        }
        Commands::Wait { id } => {
            let bp = bps
                .get(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            tokio::runtime::Builder::new_current_thread()
                .enable_io()
                .build()?
                .block_on(async {
                    bp.wait().await?;
                    Result::<(), librwmem::errors::Error>::Ok(())
                })?;
        }
        Commands::GetMemMap { pid } => {
            let map = device.get_mem_map(pid, true)?;
            println!("Memory map: {:#?}", map);
        }
        Commands::WriteMem { pid, addr, value } => {
            device.write_mem(pid, addr, &value.0)?;
        }
        Commands::ReadMem { pid, addr, size } => {
            let mut buf = vec![0u8; size as usize];
            device.read_mem(pid, addr, &mut buf)?;
            println!("Data:\n{}", pretty_hex::pretty_hex(&buf));
        }
        Commands::Step { id } => {
            let bp = bps
                .get(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            bp.step()?;
        }
        Commands::IsStopped { id } => {
            let bp = bps
                .get(&id)
                .ok_or_else(|| anyhow::anyhow!("Breakpoint not found"))?;
            let stopped = bp.is_stopped()?;
            println!("Stopped: {:?}", stopped);
        }
    }
    Ok(())
}

fn read_and_run(device: &Device, bps: &mut HashMap<i32, Breakpoint>) -> anyhow::Result<()> {
    print!("> ");
    std::io::stdout().flush().unwrap();
    let mut line = String::new();
    std::io::stdin().read_line(&mut line)?;
    if line.trim().is_empty() {
        println!();
        return Ok(());
    }
    let mut ast = shellish_parse::parse(&line, false)?;
    ast.insert(0, "rwmem".to_string());
    let cli = Cli::try_parse_from(&ast)?;

    if let Err(e) = run_cmd(cli.command, device, bps) {
        eprintln!("Error: {:?}", e);
    }
    Ok(())
}

fn main() -> anyhow::Result<()> {
    let device = Device::new(librwmem::DEFAULT_DRIVER_PATH).unwrap();
    println!("Brps: {:?}", device.get_num_brps());
    println!("Wrps: {:?}", device.get_num_wrps());
    let mut bps: HashMap<i32, Breakpoint> = HashMap::new();
    loop {
        if let Err(e) = read_and_run(&device, &mut bps) {
            eprintln!("Error: {:?}", e);
        };
    }
}
