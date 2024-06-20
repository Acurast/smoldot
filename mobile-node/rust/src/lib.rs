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

//! Contains a light client implementation usable from a mobile environment.

#![deny(rustdoc::broken_intra_doc_links)]
#![deny(unused_crate_dependencies)]

extern crate alloc;
extern crate core;

use alloc::{
    boxed::Box,
    string::{String, ToString as _},
    sync::Arc,
    vec::Vec,
};
use async_lock::Mutex;
use core::{num::NonZeroU32, pin::Pin, str, task};
use std::ptr::null;
use futures_util::{stream, Stream as _, StreamExt as _};
use smoldot_light::HandleRpcError;
use smoldot_light::platform::PlatformRef;

pub mod bindings;

mod init;
mod platform;

fn client() -> &'static Mutex<init::Client<Arc<smoldot_light::platform::DefaultPlatform>, ()>> {
    static CLIENT: async_lock::OnceCell<Mutex<init::Client<Arc<smoldot_light::platform::DefaultPlatform>, ()>>> = async_lock::OnceCell::new();

    CLIENT.get_or_init_blocking(|| {
        Mutex::new(init::Client {
            smoldot: smoldot_light::Client::new(platform::platform_ref().clone()),
            chains: slab::Slab::new(),
        })
    })
}

fn init(max_log_level: u32) {
    init::init(max_log_level);
}

fn add_chain(
    chain_spec: Vec<u8>,
    database_content: Vec<u8>,
    json_rpc_max_pending_requests: u32,
    json_rpc_max_subscriptions: u32,
    potential_relay_chains: Vec<u8>,
) -> u32 {
    let mut client_lock = futures_lite::future::block_on(client().lock());

    // Retrieve the potential relay chains parameter passed through the FFI layer.
    // TODO: this is kind of racy, as the API user could remove the relay chain while adding a parachain; it would be stupid to do that so this issue is low priority, and this code will likely change again in the future so it's not worth solving immediately
    let potential_relay_chains: Vec<_> = {
        assert_eq!(potential_relay_chains.len() % 4, 0);
        potential_relay_chains
            .chunks(4)
            .map(|c| u32::from_le_bytes(<[u8; 4]>::try_from(c).unwrap()))
            .filter_map(|c| {
                if let Some(init::Chain::Created {
                                smoldot_chain_id, ..
                            }) = client_lock.chains.get(usize::try_from(c).ok()?)
                {
                    Some(*smoldot_chain_id)
                } else {
                    None
                }
            })
            .collect()
    };

    // This function only allocates a "chain id", then spawns a task that performs the actual
    // chain creation in the background.
    // This makes it possible to measure the CPU usage of chain creation the same way as the CPU
    // is measured for all other background tasks.
    // It also makes it possible in the future to make chain creation asynchronous in the
    // `light-base` crate, which will make it possible to periodically yield and avoid using too
    // much CPU at once.
    // TODO: act on that last sentence ^
    let outer_chain_id = client_lock.chains.insert(init::Chain::Initializing);
    let outer_chain_id_u32 = u32::try_from(outer_chain_id).unwrap();

    platform::platform_ref().spawn_task(
        format!("add-chain-{outer_chain_id_u32}").into(),
        async move {
            let mut client_lock = futures_lite::future::block_on(client().lock());

            // Insert the chain in the client.
            let smoldot_light::AddChainSuccess {
                chain_id: smoldot_chain_id,
                json_rpc_responses,
            } = match client_lock
                .smoldot
                .add_chain(smoldot_light::AddChainConfig {
                    user_data: (),
                    specification: str::from_utf8(&chain_spec)
                        .unwrap_or_else(|_| panic!("non-utf8 chain spec")),
                    database_content: str::from_utf8(&database_content)
                        .unwrap_or_else(|_| panic!("non-utf8 database content")),
                    json_rpc: if let Some(json_rpc_max_pending_requests) =
                        NonZeroU32::new(json_rpc_max_pending_requests)
                    {
                        smoldot_light::AddChainConfigJsonRpc::Enabled {
                            max_pending_requests: json_rpc_max_pending_requests,
                            // Note: the PolkadotJS UI is very heavy in terms of subscriptions.
                            max_subscriptions: json_rpc_max_subscriptions,
                        }
                    } else {
                        smoldot_light::AddChainConfigJsonRpc::Disabled
                    },
                    potential_relay_chains: potential_relay_chains.into_iter(),
                }) {
                Ok(c) => c,
                Err(error) => {
                    client_lock.chains.remove(outer_chain_id);
                    unsafe {
                        let error = error.to_string();
                        bindings::chain_initialized(
                            outer_chain_id_u32,
                            error.as_bytes().as_ptr(),
                            error.as_bytes().len(),
                        );
                    }
                    return;
                }
            };

            client_lock.chains[outer_chain_id] = init::Chain::Created {
                smoldot_chain_id,
                json_rpc_response: None,
                json_rpc_response_info: Box::new(bindings::JsonRpcResponseInfo { ptr: null(), len: 0 }),
                json_rpc_responses_rx: None,
            };

            // We wrap the JSON-RPC responses stream into a proper stream in order to be able to
            // guarantee that `poll_next()` always operates on the same future.
            let json_rpc_responses = json_rpc_responses.map(|json_rpc_responses| {
                stream::unfold(json_rpc_responses, |mut json_rpc_responses| async {
                    // The stream ends when we remove the chain. Once the chain is removed, the user
                    // cannot poll the stream anymore. Therefore, it is safe to unwrap the result here.
                    let msg = json_rpc_responses.next().await.unwrap();
                    Some((msg, json_rpc_responses))
                })
                    .boxed()
            });

            if let init::Chain::Created {
                json_rpc_responses_rx,
                ..
            } = client_lock.chains.get_mut(outer_chain_id).unwrap()
            {
                *json_rpc_responses_rx = json_rpc_responses;
            }

            unsafe {
                bindings::chain_initialized(outer_chain_id_u32, null(), 0);
            }

        },
    );

    outer_chain_id_u32
}

fn remove_chain(chain_id: u32) {
    let mut client_lock = futures_lite::future::block_on(client().lock());

    match client_lock
        .chains
        .try_remove(usize::try_from(chain_id).unwrap())
    {
        Some(init::Chain::Created {
            smoldot_chain_id,
            json_rpc_responses_rx,
            ..
        }) => {
            // We've polled the JSON-RPC receiver with a waker that calls
            // `json_rpc_responses_non_empty`. Once the sender is destroyed, this waker will be
            // called in order to inform of the destruction. We don't want that to happen.
            // Therefore, we poll the receiver again with a dummy "no-op" waker for the sole
            // purpose of erasing the previously-registered waker.
            if let Some(mut json_rpc_responses_rx) = json_rpc_responses_rx {
                let _ = Pin::new(&mut json_rpc_responses_rx).poll_next(
                    &mut task::Context::from_waker(futures_util::task::noop_waker_ref()),
                );
            }

            let () = client_lock.smoldot.remove_chain(smoldot_chain_id);
        }
        Some(init::Chain::Initializing) => {} // TODO: /!\
        _ => { /* no action required */ }
    }
}

fn json_rpc_send(json_rpc_request: Vec<u8>, chain_id: u32) -> u32 {
    // As mentioned in the documentation, the bytes *must* be valid UTF-8.
    let json_rpc_request: String = String::from_utf8(json_rpc_request)
        .unwrap_or_else(|_| panic!("non-UTF-8 JSON-RPC request"));

    let mut client_lock = futures_lite::future::block_on(client().lock());
    let client_chain_id = match client_lock
        .chains
        .get(usize::try_from(chain_id).unwrap())
        .unwrap()
    {
        init::Chain::Created {
            smoldot_chain_id, ..
        } => *smoldot_chain_id,
        init::Chain::Initializing => panic!(), // Forbidden.
    };

    match client_lock
        .smoldot
        .json_rpc_request(json_rpc_request, client_chain_id)
    {
        Ok(()) => 0,
        Err(HandleRpcError::TooManyPendingRequests { .. }) => 1,
    }
}

fn json_rpc_responses_peek(chain_id: u32) -> *const bindings::JsonRpcResponseInfo {
    let mut client_lock = futures_lite::future::block_on(client().lock());
    match client_lock
        .chains
        .get_mut(usize::try_from(chain_id).unwrap())
        .unwrap()
    {
        init::Chain::Created {
            json_rpc_response,
            json_rpc_responses_rx,
            json_rpc_response_info,
            ..
        } => {
            if json_rpc_response.is_none() {
                if let Some(json_rpc_responses_rx) = json_rpc_responses_rx.as_mut() {
                    loop {
                        match Pin::new(&mut *json_rpc_responses_rx).poll_next(
                            &mut task::Context::from_waker(
                                &Arc::new(JsonRpcResponsesNonEmptyWaker { chain_id }).into(),
                            ),
                        ) {
                            task::Poll::Ready(Some(response)) if response.is_empty() => {
                                // The API of `json_rpc_responses_peek` says that a length of 0
                                // indicates that the queue is empty. For this reason, we skip
                                // this response.
                                // This is a pretty niche situation, but at least we handle it
                                // properly.
                            }
                            task::Poll::Ready(Some(response)) => {
                                debug_assert!(!response.is_empty());
                                *json_rpc_response = Some(response);
                                break;
                            }
                            task::Poll::Ready(None) => unreachable!(),
                            task::Poll::Pending => break,
                        }
                    }
                }
            }

            // Note that we might be returning the last item in the queue. In principle, this means
            // that the next time an entry is added to the queue, `json_rpc_responses_non_empty`
            // should be called. Due to the way the implementation works, this will not happen
            // until the user calls `json_rpc_responses_peek`. However, this is not a problem:
            // it is impossible for the user to observe that the queue is empty, and as such there
            // is simply not correct implementation of the API that can't work because of this
            // property.

            match &json_rpc_response {
                Some(rp) => {
                    debug_assert!(!rp.is_empty());
                    json_rpc_response_info.ptr = rp.as_bytes().as_ptr();
                    json_rpc_response_info.len = rp.as_bytes().len();
                }
                None => {
                    json_rpc_response_info.ptr = null();
                    json_rpc_response_info.len = 0;
                }
            }

            (&**json_rpc_response_info) as *const bindings::JsonRpcResponseInfo
        }
        _ => panic!(),
    }
}

fn json_rpc_responses_pop(chain_id: u32) {
    let mut client_lock = futures_lite::future::block_on(client().lock());
    match client_lock
        .chains
        .get_mut(usize::try_from(chain_id).unwrap())
        .unwrap()
    {
        init::Chain::Created {
            json_rpc_response, ..
        } => *json_rpc_response = None,
        _ => panic!(),
    }
}

struct JsonRpcResponsesNonEmptyWaker {
    chain_id: u32,
}

impl alloc::task::Wake for JsonRpcResponsesNonEmptyWaker {
    fn wake(self: Arc<Self>) {
        unsafe { bindings::json_rpc_responses_non_empty(self.chain_id) }
    }
}
