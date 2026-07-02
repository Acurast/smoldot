use alloc::sync::Arc;
use std::panic;
use std::sync::atomic::{AtomicU32, Ordering};
use crate::bindings;

pub(crate) fn platform_ref() -> &'static Arc<smoldot_light::platform::DefaultPlatform> {
    static PLATFORM_REF: async_lock::OnceCell<Arc<smoldot_light::platform::DefaultPlatform>> = async_lock::OnceCell::new();

    PLATFORM_REF.get_or_init_blocking(|| {
        smoldot_light::platform::DefaultPlatform::new(
            env!("CARGO_PKG_NAME").into(),
            env!("CARGO_PKG_VERSION").into(),
        )
    })
}

pub(crate) fn panic_handler(info: &panic::PanicInfo) {
    let message = alloc::string::ToString::to_string(info);

    unsafe {
        bindings::panic(
            message.as_bytes().as_ptr(),
            message.as_bytes().len(),
        );
        unreachable!();
    }
}


pub(crate) struct Logger {
    /// Log level above which log entries aren't emitted.
    max_log_level: AtomicU32,
}

pub(crate) static LOGGER: Logger = Logger::new(0);

impl Logger {
    pub const fn new(max_log_level: u32) -> Self {
        Logger {
            max_log_level: AtomicU32::new(max_log_level),
        }
    }

    pub fn set_max_level(&self, max_log_level: u32) {
        self.max_log_level.store(max_log_level, Ordering::SeqCst);
    }
}

/// Implementation of [`log::Log`] that sends out logs to the FFI.
impl log::Log for Logger {
    fn enabled(&self, _: &log::Metadata) -> bool {
        true
    }

    fn log(&self, record: &log::Record) {
        let log_level = match record.level() {
            log::Level::Error => 1,
            log::Level::Warn => 2,
            log::Level::Info => 3,
            log::Level::Debug => 4,
            log::Level::Trace => 5,
        };

        if log_level > self.max_log_level.load(Ordering::Relaxed) {
            return;
        }

        let target = record.target();
        let message = format!("{}", record.args());

        unsafe {
            bindings::print(
                record.level() as u32,
                target.as_bytes().as_ptr(),
                target.as_bytes().len(),
                message.as_bytes().as_ptr(),
                message.as_bytes().len(),
            )
        }
    }

    fn flush(&self) {}
}