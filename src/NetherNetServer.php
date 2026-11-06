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

use NetherNet\NetherNetException;
use NetherNet\Server;
use InvalidArgumentException;
use RuntimeException;

class NetherNetServer
{
	private Server $server;

	/** @var NetherNetSession[] */
	private array $sessions = [];

	public const SCHEME_HTTP = "http";
	public const SCHEME_HTTPS = "https";

	public const MAX_EVENTS_PER_TICK = 1024;
	public const MAX_TICK_NS = 5_000_000;

	private string $url;

	/**
	 * @param string $url signaling listener. "0.0.0.0:19132" picks the scheme itself: https when a certificate and key
	 *				are given, http otherwise. "http://..." / "https://..." force one.
	 * @param string $operator_key_path P-384 PEM, generated when missing. keep it stable (clients pin it)
	 * @param string $operator_domain idp.domain sent in the answer identity
	 * @param bool $operator_cpk_spki the server key as base64 SPKI (true, what real clients accept and remember)
	 *								or as a JWK object (false: clients re-ask trust every join)
	 * @param ?string $tls_cert_path PEM certificate (full chain) for https, a trusted one avoids the "trust this server?" prompt
	 * @param ?string $tls_key_path its PEM private key
	 * @param int $ice_port UDP port all WebRTC traffic shares (0 = a random one per process). Set it to a port your
	 *						firewall / DDoS protection lets through (with a NetherNet profile where the host has one)
	 * @param array{mtu?: int, advertise_addresses?: string|list<string>, trusted_proxies?: string|list<string>, real_ip_header?: string} $network_options
	 *				mtu: WebRTC packet size, 0 = default (lower it to 1000/900 for players on VPN, PPPoE or mobile data).
	 *				advertise_addresses: public IPs offered next to the local ones, same port (NAT, Docker, cloud VMs; needs a
	 *				1:1 UDP port forward of ice_port). trusted_proxies: reverse proxies in front of signaling, whose requests
	 *				carry the player's IP in real_ip_header ("X-Forwarded-For" by default) and are refused without it.
	 */
	public function __construct(
		string $url,
		private ServerInformation $information,
		private NetherNetListener $listener,
		string $operator_key_path = "operator_key.pem",
		string $operator_domain = "",
		bool $operator_cpk_spki = true,
		bool $allow_unauthenticated = false,
		?string $tls_cert_path = null,
		?string $tls_key_path = null,
		int $ice_port = 0,
		array $network_options = []
	) {
		if ($ice_port < 0 || $ice_port > 65535) {
			throw new InvalidArgumentException("ice_port must be 0 (random) or a UDP port number, got $ice_port");
		}
		if (!extension_loaded("nethernet")) {
			throw new RuntimeException("The nethernet extension is not loaded (extension=php_nethernet.dll)");
		}
		$tls_cert_path = $tls_cert_path === "" ? null : $tls_cert_path;
		$tls_key_path = $tls_key_path === "" ? null : $tls_key_path;
		$this->url = self::resolveUrl($url, $tls_cert_path, $tls_key_path);
		$this->server = new Server([
			"operator_key_path" => $operator_key_path,
			"operator_domain" => $operator_domain !== "" ? $operator_domain : null,
			"operator_cpk_spki" => $operator_cpk_spki,
			"allow_unauthenticated" => $allow_unauthenticated,
			"tls_cert_path" => $tls_cert_path,
			"tls_key_path" => $tls_key_path,
			"ice_port" => $ice_port
		] + self::networkOptions($network_options));
	}

	/**
	 * @param array<string, mixed> $options
	 * @return array<string, int|string>
	 */
	private static function networkOptions(array $options): array
	{
		$out = [];
		foreach ($options as $key => $value) {
			switch ($key) {
				case "mtu":
					if (!is_int($value)) {
						throw new InvalidArgumentException("mtu must be an int");
					}
					if ($value !== 0) {
						$out["mtu"] = $value;
					}
					break;
				case "advertise_addresses":
				case "trusted_proxies":
				case "real_ip_header":
					if (is_array($value) && $key !== "real_ip_header") {
						foreach ($value as $entry) {
							if (!is_string($entry)) {
								throw new InvalidArgumentException("$key must only contain strings");
							}
						}
						$value = implode(",", $value);
					}
					if (!is_string($value)) {
						throw new InvalidArgumentException("$key must be a string");
					}
					if ($value !== "") {
						$out[$key] = $value;
					}
					break;
				default:
					throw new InvalidArgumentException("Unknown network option \"$key\"");
			}
		}
		return $out;
	}

	public static function resolveUrl(string $url, ?string $tls_cert_path, ?string $tls_key_path): string
	{
		$has_tls = $tls_cert_path !== null || $tls_key_path !== null;
		if ($has_tls && ($tls_cert_path === null || $tls_key_path === null)) {
			throw new InvalidArgumentException("https needs both a certificate and a key");
		}
		if (preg_match('#^([a-z]+)://#i', $url, $m) !== 1) {
			return ($has_tls ? self::SCHEME_HTTPS : self::SCHEME_HTTP) . "://" . $url;
		}
		$scheme = strtolower($m[1]);
		if ($scheme === self::SCHEME_HTTPS && !$has_tls) {
			throw new InvalidArgumentException("https:// needs a certificate and a key (tls_cert_path / tls_key_path)");
		}
		if ($scheme === self::SCHEME_HTTP && $has_tls) {
			throw new InvalidArgumentException("a certificate was given but the url is http://, use https:// or no scheme");
		}
		if ($scheme !== self::SCHEME_HTTP && $scheme !== self::SCHEME_HTTPS) {
			throw new InvalidArgumentException("unsupported signaling scheme: $scheme");
		}
		return $url;
	}

	public function getUrl(): string
	{
		return $this->url;
	}

	public function isHttps(): bool
	{
		return str_starts_with(strtolower($this->url), "https://");
	}

	/**
	 * @throws NetherNetException
	 */
	public function start(): void
	{
		$this->server->start($this->url, $this->information->toArray());
		$this->information->markClean();
	}

	public function isRunning(): bool
	{
		return $this->server->isRunning();
	}

	public function setBannedNames(array $names): void
	{
		$this->server->setBannedNames($names);
	}

	public function setBannedIps(array $ips): void
	{
		$this->server->setBannedIps($ips);
	}

	public function setWhitelist(array $names): void
	{
		$this->server->setWhitelist($names);
	}

	public function setWhitelistEnabled(bool $enabled): void
	{
		$this->server->setWhitelistEnabled($enabled);
	}

	public function setRejectPolicy(bool $banned, bool $notWhitelisted, bool $whenFull): void
	{
		$this->server->setRejectPolicy($banned, $notWhitelisted, $whenFull);
	}

	public function setCapacity(int $players, int $maxPlayers): void
	{
		$this->server->setCapacity($players, $maxPlayers);
	}

	public function getInformation(): ServerInformation
	{
		return $this->information;
	}

	/**
	 * @return NetherNetSession[]
	 */
	public function getSessions(): array
	{
		return $this->sessions;
	}

	public function getSession(int $id): ?NetherNetSession
	{
		return $this->sessions[$id] ?? null;
	}

	public function tick(): void
	{
		if (!$this->server->isRunning()) {
			return;
		}

		if ($this->information->isDirty()) {
			$this->server->setInformation($this->information->toArray());
			$this->information->markClean();
		}

		$this->server->update();
		$deadline = hrtime(true) + self::MAX_TICK_NS;
		for ($handled = 0; $handled < self::MAX_EVENTS_PER_TICK && hrtime(true) < $deadline; ++$handled) {
			$event = $this->server->pollEvent();
			if ($event === null) {
				break;
			}
			$this->handleEvent($event);
		}
	}

	private function handleEvent(array $event): void
	{
		$id = $event["id"];
		switch ($event["type"]) {
			case Server::EVENT_CONNECTED:
				$session = new NetherNetSession($this, $id, $event["networkId"], $event["xuid"], $event["name"], $event["address"] ?? "", $event["port"] ?? 0);
				$this->sessions[$id] = $session;
				$this->listener->onConnect($session);
				break;
			case Server::EVENT_DATA:
				if (isset($this->sessions[$id])) {
					$this->listener->onPacket($this->sessions[$id], $event["data"], $event["channel"]);
				}
				break;
			case Server::EVENT_DISCONNECTED:
				if (isset($this->sessions[$id])) {
					$session = $this->sessions[$id];
					unset($this->sessions[$id]);
					$session->markClosed();
					$this->listener->onDisconnect($session);
				}
				break;
		}
	}

	public function send(int $id, string $payload, int $channel = Server::CHANNEL_RELIABLE): int
	{
		return $this->server->send($id, $payload, $channel);
	}

	public function disconnect(int $id): void
	{
		$this->server->disconnect($id);
	}

	public function close(): void
	{
		foreach ($this->sessions as $session) {
			$session->markClosed();
		}
		$this->sessions = [];
		$this->server->close();
	}
}
