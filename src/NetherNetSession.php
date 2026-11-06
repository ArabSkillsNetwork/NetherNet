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

declare(strict_types=1);

namespace Laith98Dev\nethernet;

use NetherNet\Server;

class NetherNetSession
{
	private bool $closed = false;

	public function __construct(
		private NetherNetServer|NetherNetClient $server,
		private int $id,
		private string $network_id,
		private string $xuid,
		private string $name,
		private string $address = "",
		private int $port = 0
	) {
		// NOOP
	}

	public function getId(): int
	{
		return $this->id;
	}

	public function getNetworkId(): string
	{
		return $this->network_id;
	}

	public function getXuid(): string
	{
		return $this->xuid;
	}

	public function getName(): string
	{
		return $this->name;
	}

	/**
	 * The peer's ip: where its signaling request came from (like raknet's source address), or for client
	 * connections the address webrtc talks to. "" if unknown.
	 */
	public function getAddress(): string
	{
		return $this->address;
	}

	public function getPort(): int
	{
		return $this->port;
	}

	public function isClosed(): bool
	{
		return $this->closed;
	}

	/**
	 * @return int one of Server::SEND_* (Server::SEND_SUCCESS when sent)
	 */
	public function send(string $payload, bool $reliable = true): int
	{
		if ($this->closed) {
			return Server::SEND_NOT_FOUND;
		}
		return $this->server->send($this->id, $payload, $reliable ? Server::CHANNEL_RELIABLE : Server::CHANNEL_UNRELIABLE);
	}

	public function disconnect(): void
	{
		if (!$this->closed) {
			$this->server->disconnect($this->id);
		}
	}

	/**
	 * @internal called by NetherNetServer once the connection is gone
	 */
	public function markClosed(): void
	{
		$this->closed = true;
	}
}
