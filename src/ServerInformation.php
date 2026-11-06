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

class ServerInformation
{
	private bool $dirty = true;

	public function __construct(
		private string $name = "NetherNet Server",
		private int $protocol = 0,
		private string $version = "",
		private string $level = "world",
		private int $players = 0,
		private int $max_players = 20,
		private int $game_type = 0
	) {
		// NOOP
	}

	public function getName(): string
	{
		return $this->name;
	}

	public function setName(string $name): void
	{
		if ($this->name !== $name) {
			$this->name = $name;
			$this->dirty = true;
		}
	}

	public function getProtocol(): int
	{
		return $this->protocol;
	}

	public function setProtocol(int $protocol): void
	{
		if ($this->protocol !== $protocol) {
			$this->protocol = $protocol;
			$this->dirty = true;
		}
	}

	public function getVersion(): string
	{
		return $this->version;
	}

	public function setVersion(string $version): void
	{
		if ($this->version !== $version) {
			$this->version = $version;
			$this->dirty = true;
		}
	}

	public function getLevel(): string
	{
		return $this->level;
	}

	public function setLevel(string $level): void
	{
		if ($this->level !== $level) {
			$this->level = $level;
			$this->dirty = true;
		}
	}

	public function getPlayers(): int
	{
		return $this->players;
	}

	public function setPlayers(int $players): void
	{
		if ($this->players !== $players) {
			$this->players = $players;
			$this->dirty = true;
		}
	}

	public function getMaxPlayers(): int
	{
		return $this->max_players;
	}

	public function setMaxPlayers(int $max_players): void
	{
		if ($this->max_players !== $max_players) {
			$this->max_players = $max_players;
			$this->dirty = true;
		}
	}

	public function getGameType(): int
	{
		return $this->game_type;
	}

	public function setGameType(int $game_type): void
	{
		if ($this->game_type !== $game_type) {
			$this->game_type = $game_type;
			$this->dirty = true;
		}
	}

	/**
	 * @internal true when something changed since the last markClean()
	 */
	public function isDirty(): bool
	{
		return $this->dirty;
	}

	/**
	 * @internal
	 */
	public function markClean(): void
	{
		$this->dirty = false;
	}

	public function toArray(): array
	{
		return [
			"name" => $this->name,
			"protocol" => $this->protocol,
			"version" => $this->version,
			"level" => $this->level,
			"players" => $this->players,
			"maxPlayers" => $this->max_players,
			"gameType" => $this->game_type
		];
	}
}
