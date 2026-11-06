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

use RuntimeException;

class FingerprintPinStore
{
	/** @var array<string, string> server url => fingerprint */
	private array $pins = [];

	public function __construct(private string $path)
	{
		if (is_file($this->path)) {
			$data = json_decode(strval(file_get_contents($this->path)), true);
			if (is_array($data)) {
				foreach ($data as $server_url => $fingerprint) {
					if (is_string($server_url) && is_string($fingerprint)) {
						$this->pins[$server_url] = $fingerprint;
					}
				}
			}
		}
	}

	public function get(string $server_url): ?string
	{
		return $this->pins[strtolower(rtrim($server_url, "/"))] ?? null;
	}

	public function pin(string $server_url, string $fingerprint): void
	{
		$this->pins[strtolower(rtrim($server_url, "/"))] = $fingerprint;
		$this->save();
	}

	public function forget(string $server_url): void
	{
		unset($this->pins[strtolower(rtrim($server_url, "/"))]);
		$this->save();
	}

	private function save(): void
	{
		$tmp = null;
		try {
			$json = json_encode($this->pins, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
			if ($json === false) {
				throw new RuntimeException();
			}
			$tmp = $this->path . "." . bin2hex(random_bytes(4)) . ".tmp";
			if (file_put_contents($tmp, $json, LOCK_EX) === false || !rename($tmp, $this->path)) {
				throw new RuntimeException();
			}
		} catch (\Throwable) {
			if ($tmp !== null) {
				@unlink($tmp);
			}
			throw new RuntimeException("Unable to write the pin store: " . $this->path);
		}
	}
}
