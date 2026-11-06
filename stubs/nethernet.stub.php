<?php

/*
 * MIT License
 *
 * Copyright (c) 2026 vp817
 * Copyright (c) 2026 Laith98Dev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

namespace NetherNet;

class NetherNetException extends \RuntimeException
{
}

final class Server
{
	public const CHANNEL_RELIABLE = 0;
	public const CHANNEL_UNRELIABLE = 1;

	public const EVENT_CONNECTED = 1;
	public const EVENT_DATA = 2;
	public const EVENT_DISCONNECTED = 3;

	public const SEND_SUCCESS = 0;
	public const SEND_NOT_FOUND = 11;
	public const SEND_NOT_READY = 12;
	public const SEND_TOO_LARGE = 13;
	public const SEND_FAILED = 14;

	/**
	 * @param array{
	 *     operator_key_path?: string|null,
	 *     operator_domain?: string|null,
	 *     operator_cpk_spki?: bool,
	 *     allow_unauthenticated?: bool,
	 *     tls_cert_path?: string|null,
	 *     tls_key_path?: string|null,
	 *     max_pending?: int,
	 *     max_pending_per_ip?: int,
	 *     max_connections_per_ip?: int,
	 *     ice_port?: int,
	 *     mtu?: int,
	 *     advertise_addresses?: string|null,
	 *     trusted_proxies?: string|null,
	 *     real_ip_header?: string|null
	 * } $options
	 */
	public function __construct(array $options = []) {}

	/**
	 * @param string $url "http://host:port" or "https://host:port" (https needs tls_cert_path + tls_key_path)
	 * @param array<string, mixed> $information GET /v1/join body (name, protocol, version, level, players, maxPlayers, gameType)
	 * @throws NetherNetException
	 */
	public function start(string $url, array $information): void {}

	/** @param array<string, mixed> $information */
	public function setInformation(array $information): void {}

	public function update(): void {}

	/**
	 * @return array{type: int, id: int, networkId: string, xuid: string, name: string, address: string, port: int, channel: int, data?: string}|null
	 */
	public function pollEvent(): ?array {}

	/** @return int one of the SEND_* constants */
	public function send(int $id, string $payload, int $channel = self::CHANNEL_RELIABLE): int {}

	public function disconnect(int $id): bool {}

	public function close(): void {}

	public function isRunning(): bool {}

	/** @param list<string> $names */
	public function setBannedNames(array $names): void {}

	/** @param list<string> $ips */
	public function setBannedIps(array $ips): void {}

	/** @param list<string> $names */
	public function setWhitelist(array $names): void {}

	public function setWhitelistEnabled(bool $enabled): void {}

	public function setRejectPolicy(bool $banned, bool $notWhitelisted, bool $whenFull): void {}

	public function setCapacity(int $players, int $maxPlayers): void {}
}

final class Client
{
	/** @return array<string, mixed>|null GET {url}/v1/join */
	public function query(string $url): ?array {}

	/**
	 * Blocks until both data channels are open.
	 *
	 * @param array{
	 *     token?: string|null,
	 *     key_pem?: string|null,
	 *     network_id?: string|null,
	 *     idp_domain?: string|null,
	 *     expected_fingerprint?: string|null,
	 *     allow_missing_identity?: bool,
	 *     timeout_ms?: int
	 * } $options
	 * @return array{id: int, fingerprint: string}
	 * @throws NetherNetException
	 */
	public function connect(string $url, array $options = []): array {}

	public function update(): void {}

	/** @see Server::pollEvent() */
	public function pollEvent(): ?array {}

	public function send(int $id, string $payload, int $channel = Server::CHANNEL_RELIABLE): int {}

	public function disconnect(int $id): bool {}

	public function close(): void {}
}
