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

use NetherNet\Client;
use NetherNet\NetherNetException;
use NetherNet\Server;
use RuntimeException;

class NetherNetClient
{
	private Client $client;

	/** @var NetherNetSession[] */
	private array $sessions = [];

	private array $fingerprints = [];

	public function __construct(
		private NetherNetListener $listener,
		private ?FingerprintPinStore $pins = null
	) {
		if (!extension_loaded("nethernet")) {
			throw new RuntimeException("The nethernet extension is not loaded (extension=php_nethernet.dll)");
		}
		$this->client = new Client();
	}

	public function query(string $url): ?ServerInformation
	{
		$info = $this->client->query($url);
		if ($info === null) {
			return null;
		}
		return new ServerInformation(
			strval($info["name"] ?? ""),
			intval($info["protocol"] ?? 0),
			strval($info["version"] ?? ""),
			strval($info["level"] ?? ""),
			intval($info["players"] ?? 0),
			intval($info["maxPlayers"] ?? 0),
			intval($info["gameType"] ?? 0)
		);
	}

	/**
	 * @throws NetherNetException
	 */
	public function connect(string $url, ?string $game_server_token = null, ?string $client_key_pem = null, int $timeout_ms = 15000, bool $allow_missing_identity = false, ?string $expected_fingerprint = null): NetherNetSession
	{
		$url = rtrim($url, "/");
		$plain_http = str_starts_with(strtolower($url), "http://");
		if ($expected_fingerprint === null && $plain_http && $this->pins !== null) {
			$expected_fingerprint = $this->pins->get($url);
		}

		$result = $this->client->connect($url, [
			"token" => $game_server_token,
			"key_pem" => $client_key_pem,
			"timeout_ms" => $timeout_ms,
			"allow_missing_identity" => $allow_missing_identity,
			"expected_fingerprint" => $expected_fingerprint
		]);
		$id = $result["id"];
		$fingerprint = $result["fingerprint"];

		if ($plain_http && $this->pins !== null && $fingerprint !== "" && $this->pins->get($url) === null) {
			$this->pins->pin($url, $fingerprint);
		}

		$session = new NetherNetSession($this, $id, "", "", "");
		$this->sessions[$id] = $session;
		$this->fingerprints[$id] = $fingerprint;
		return $session;
	}

	public function getServerFingerprint(NetherNetSession $session): string
	{
		return $this->fingerprints[$session->getId()] ?? "";
	}

	/**
	 * @return NetherNetSession[]
	 */
	public function getSessions(): array
	{
		return $this->sessions;
	}

	public function tick(): void
	{
		$this->client->update();
		while (($event = $this->client->pollEvent()) !== null) {
			$id = $event["id"];
			$session = $this->sessions[$id] ?? null;
			if ($session === null) {
				continue;
			}
			switch ($event["type"]) {
				case Server::EVENT_CONNECTED:
					$this->listener->onConnect($session);
					break;
				case Server::EVENT_DATA:
					$this->listener->onPacket($session, $event["data"], $event["channel"]);
					break;
				case Server::EVENT_DISCONNECTED:
					unset($this->sessions[$id], $this->fingerprints[$id]);
					$session->markClosed();
					$this->listener->onDisconnect($session);
					break;
			}
		}
	}

	public function send(int $id, string $payload, int $channel = Server::CHANNEL_RELIABLE): int
	{
		return $this->client->send($id, $payload, $channel);
	}

	public function disconnect(int $id): void
	{
		$this->client->disconnect($id);
	}

	public function close(): void
	{
		foreach ($this->sessions as $session) {
			$session->markClosed();
		}
		$this->sessions = [];
		$this->fingerprints = [];
		$this->client->close();
	}
}
