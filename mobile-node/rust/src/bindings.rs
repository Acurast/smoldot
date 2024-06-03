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

//! Imports and exports of the C++ module.
//!
//! This module contains all the functions that tie together the Rust code and its host (i.e.
//! the C++ code, normally).
//!
//! The functions found in the `extern` block are the functions that the Rust code *imports*, and
//! need to be implemented on the host side and provided to the C++ environment. The
//! other functions are functions that the Rust code *exports*, and can be called by the host.
//!
//! # Re-entrency
//!
//! As a rule, none of the implementations of the functions that the host provides is allowed
//! to call a function exported by Rust.
//!
//! This avoids potential stack overflows and tricky borrowing-related situations.

use std::slice;

extern "C" {
    /// Must stop the execution immediately. The message is a UTF-8 string found
    /// in the memory at offset `message_ptr` and with length `message_len`.
    pub fn panic(message_ptr: *const u8, message_len: usize);

    /// Called in response to [`add_chain`] once the initialization of the chain is complete.
    ///
    /// If `error_msg_ptr` is equal to 0, then the chain initialization is successful. Otherwise,
    /// `error_msg_ptr` and `error_msg_len` designate a buffer in the memory of the WebAssembly
    /// virtual machine where a UTF-8 diagnostic error message can be found.
    pub fn chain_initialized(chain_id: u32, error_msg_ptr: *const u8, error_msg_len: usize);

    /// The queue of JSON-RPC responses of the given chain is no longer empty.
    ///
    /// This function is only ever called after [`json_rpc_responses_peek`] has returned a `len`
    /// of 0.
    ///
    /// This function might be called spuriously, however this behavior must not be relied upon.
    pub fn json_rpc_responses_non_empty(chain_id: u32);

    /// Client is emitting a log entry.
    ///
    /// Each log entry is made of a log level (`1 = Error, 2 = Warn, 3 = Info, 4 = Debug,
    /// 5 = Trace`), a log target (e.g. "network"), and a log message.
    ///
    /// The log target and message is a UTF-8 string found in the memory at offset `ptr` and with length `len`.
    pub fn print(
        level: u32,
        target_ptr: *const u8,
        target_len: usize,
        message_ptr: *const u8,
        message_len: usize,
    );
}

/// Initializes the client.
///
/// This is the first function that must be called. Failure to do so before calling another
/// method will lead to a Rust panic. Calling this function multiple times will also lead to a
/// panic.
///
/// The client will emit log messages by calling the [`print()`] function, provided the log level is
/// inferior or equal to the value of `max_log_level` passed here.
#[no_mangle]
pub extern "C" fn init(max_log_level: u32) {
    crate::init(max_log_level);
}

/// Adds a chain to the client. The client will try to stay connected and synchronize this chain.
///
/// The content of the chain specification and database content must be in UTF-8.
///
/// > **Note**: The database content is an opaque string that can be obtained by calling
/// >           the `chainHead_unstable_finalizedDatabase` JSON-RPC function.
///
/// The list of potential relay chains is a buffer containing a list of 32-bits-little-endian chain
/// ids. If the chain specification refer to a parachain, these chain ids are the ones that will be
/// looked up to find the corresponding relay chain.
///
/// `json_rpc_max_pending_requests` indicates the size of the queue of JSON-RPC requests that
/// haven't been answered yet.
/// If `json_rpc_max_pending_requests` is 0, then no JSON-RPC service will be started and it is
/// forbidden to send JSON-RPC requests targeting this chain. This can be used to save up
/// resources.
/// If `json_rpc_max_pending_requests` is 0, then the value of `json_rpc_max_subscriptions` is
/// ignored.
///
/// Calling this function allocates a chain id and starts the chain initialization in the
/// background. Once the initialization is complete, the [`chain_initialized`] function will be
/// called by smoldot.
/// It is possible to call [`remove_chain`] while the initialization is still in progress in
/// order to cancel it.
#[no_mangle]
pub extern "C" fn add_chain(
    chain_spec_buffer_ptr: *const u8,
    chain_spec_buffer_len: usize,
    database_content_buffer_ptr: *const u8,
    database_content_buffer_len: usize,
    json_rpc_max_pending_requests: u32,
    json_rpc_max_subscriptions: u32,
    potential_relay_chains_buffer_ptr: *const u8,
    potential_relay_chains_buffer_len: usize,
) -> u32 {
    super::add_chain(
        get_buffer(chain_spec_buffer_ptr, chain_spec_buffer_len),
        get_buffer(database_content_buffer_ptr, database_content_buffer_len),
        json_rpc_max_pending_requests,
        json_rpc_max_subscriptions,
        get_buffer(potential_relay_chains_buffer_ptr, potential_relay_chains_buffer_len),
    )
}

/// Removes a chain previously added using [`add_chain`]. Instantly unsubscribes all the JSON-RPC
/// subscriptions and cancels all in-progress requests corresponding to that chain.
///
/// Can be called on a chain which hasn't finished initializing yet.
#[no_mangle]
pub extern "C" fn remove_chain(chain_id: u32) {
    super::remove_chain(chain_id);
}

/// Emit a JSON-RPC request or notification towards the given chain previously added using
/// [`add_chain`].
///
/// A buffer containing a UTF-8 JSON-RPC request or notification must be passed as parameter. The
/// format of the JSON-RPC requests and notifications is described in
/// [the standard JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification).
///
/// If the buffer isn't a valid JSON-RPC request, then an error JSON-RPC response with an `id`
/// equal to `null` is generated, in accordance with the JSON-RPC 2.0 specification.
///
/// Responses and notifications are notified using [`json_rpc_responses_non_empty`], and can
/// be read with [`json_rpc_responses_peek`].
///
/// It is forbidden to call this function on a chain which hasn't finished initializing yet or a
/// chain that was created with `json_rpc_running` equal to 0.
///
/// This function returns:
/// - 0 on success.
/// - 1 if the chain has too many pending JSON-RPC requests and refuses to queue another one.
///
#[no_mangle]
pub extern "C" fn json_rpc_send(text_buffer_ptr: *const u8, text_buffer_len: usize, chain_id: u32) -> u32 {
    super::json_rpc_send(get_buffer(text_buffer_ptr, text_buffer_len), chain_id)
}

/// Obtains information about the first response in the queue of JSON-RPC responses.
///
/// This function returns a pointer within the memory where a struct of type [`JsonRpcResponseInfo`] is stored.
/// This pointer remains valid until [`json_rpc_responses_pop`] or [`remove_chain`] is called with the same `chain_id`.
///
/// The response or notification is a UTF-8 string found in the memory at offset `ptr` and with length `len`,
/// where `ptr` and `len` are found in the [`JsonRpcResponseInfo`].
///
/// If `len` is equal to 0, this indicates that the queue of JSON-RPC responses is empty.
/// When a `len` of 0 is returned, [`json_rpc_responses_non_empty`] will later be called to
/// indicate that it is no longer empty.
///
/// After having read the response or notification, use [`json_rpc_responses_pop`] to remove it
/// from the queue. You can then call [`json_rpc_responses_peek`] again to read the next response.
#[no_mangle]
pub extern "C" fn json_rpc_responses_peek(chain_id: u32) -> *const JsonRpcResponseInfo {
    super::json_rpc_responses_peek(chain_id)
}

/// See [`json_rpc_responses_peek`].
#[repr(C)]
pub struct JsonRpcResponseInfo {
    /// Pointer in memory where the JSON-RPC response can be found.
    pub ptr: *const u8,
    /// Length of the JSON-RPC response in bytes. If 0, indicates that the queue is empty.
    pub len: usize,
}

unsafe impl Send for JsonRpcResponseInfo {}

/// Removes the first response from the queue of JSON-RPC responses. This is the response whose
/// information can be retrieved using [`json_rpc_responses_peek`].
///
/// Calling this function invalidates the pointer previously returned by a call to
/// [`json_rpc_responses_peek`] with the same `chain_id`.
///
/// It is forbidden to call this function on a chain that hasn't finished initializing yet, or a
/// chain that was created with `json_rpc_running` equal to 0.
#[no_mangle]
pub extern "C" fn json_rpc_responses_pop(chain_id: u32) {
    super::json_rpc_responses_pop(chain_id);
}

pub(crate) fn get_buffer(ptr: *const u8, len: usize) -> Vec<u8> {
    unsafe {
        slice::from_raw_parts(ptr, len).to_vec()
    }
}
