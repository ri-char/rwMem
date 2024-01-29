use thiserror::Error;

#[derive(Debug, Error)]
pub enum Error {
    #[error("IO error: {0}")]
    Errno(#[from] nix::errno::Errno),
    #[error("read too small: expect read {0} bytes, but read {1} bytes")]
    ReadTooSmall(usize, usize),
    #[error("maps too long")]
    MapsTooLong,
    #[error("maps parse error: {0}")]
    MapsParseError(#[from] std::io::Error),
    #[error("not aligned")]
    NotAligned,
    #[error("begin address {0} is larger than end address {1}")]
    BeginLargerThanEnd(u64, u64),
}
