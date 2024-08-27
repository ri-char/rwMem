use thiserror::Error;

#[derive(Debug, Error)]
pub enum Error {
    #[error("IO error: {0}")]
    Errno(#[from] nix::errno::Errno),
    #[error("read too short: expect read {0} bytes, but short {1} bytes")]
    ReadFailed(usize, usize),
    #[error("write too short: expect write {0} bytes, but short {1} bytes")]
    WriteFailed(usize, usize),
    #[error("maps too long")]
    MapsTooLong,
    #[error("maps parse error: {0}")]
    MapsParseError(#[from] std::io::Error),
    #[error("not aligned")]
    NotAligned,
    #[error("begin address {0} is larger than end address {1}")]
    BeginLargerThanEnd(u64, u64),
    #[error("invalid register: {0}")]
    InvalidRegister(u64),
    #[error("invalid breakpoint type: {0}")]
    InvalidBreakpointType(String),
}
