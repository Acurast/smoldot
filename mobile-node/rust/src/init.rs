// Smoldot
// Copyright (C) 2019-2022  Parity Technologies (UK) Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later WITH Classpath-exception-2.0

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

use alloc::{boxed::Box, format, string::String};
use core::iter;
use core::sync::atomic::{AtomicBool, Ordering};
use std::panic;

use futures_util::stream;
use smoldot_light::platform::PlatformRef;

use crate::{bindings, platform};

pub(crate) struct Client<TPlat: PlatformRef, TChain> {
    pub(crate) smoldot: smoldot_light::Client<TPlat, TChain>,

    /// List of all chains that have been added by the user.
    pub(crate) chains: slab::Slab<Chain>,
}

pub(crate) enum Chain {
    Initializing,
    Created {
        smoldot_chain_id: smoldot_light::ChainId,

        /// JSON-RPC responses that is at the front of the queue according to the API. If `Some`,
        /// a pointer to the string is referenced to within
        /// [`Chain::Created::json_rpc_response_info`].
        json_rpc_response: Option<String>,
        /// Information about [`Chain::Created::json_rpc_response`]. A pointer to this struct is
        /// sent over the FFI layer to the JavaScript. As such, the pointer must never be
        /// invalidated.
        json_rpc_response_info: Box<bindings::JsonRpcResponseInfo>,
        /// Receiver for JSON-RPC responses sent by the client. `None` if JSON-RPC requests are
        /// disabled on this chain.
        /// While this could in principle be a [`smoldot_light::JsonRpcResponses`], we wrap it
        /// within a [`futures_util::Stream`] in order to guarantee that the `waker` that we
        /// register doesn't get cleaned up.
        json_rpc_responses_rx: Option<stream::BoxStream<'static, String>>,
    },
}

pub(crate) fn init(max_log_level: u32) {
    // The maximum log level can be updated on every call (e.g. after a destroy/re-init).
    platform::LOGGER.set_max_level(max_log_level);

    // The panic hook and logger are process-global and survive a destroy/re-init, so they must
    // only be installed once. Re-installing the panic hook would clobber any hook set elsewhere
    // in the host process.
    static INITIALIZED: AtomicBool = AtomicBool::new(false);
    if INITIALIZED.swap(true, Ordering::SeqCst) {
        return;
    }

    panic::set_hook(Box::new(platform::panic_handler));
    let _ = log::set_logger(&platform::LOGGER);

    // Print the version in order to make it easier to debug issues by reading logs provided by
    // third parties.
    platform::platform_ref().log(
        smoldot_light::platform::LogLevel::Info,
        "smoldot",
        &format!("Smoldot v{}", env!("CARGO_PKG_VERSION")),
        iter::empty(),
    );
}
