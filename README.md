# AuctionSim: A Module For AzerothCore WoTLK 3.3.5a

AuctionSim populates and maintains your realm's auction house using price data scraped from a live WoW 3.3.5a economy (Lordaeron, Warmane). Unlike the classic ah-bot module, it isn't limited to a handful of item categories and doesn't price items based on their vendor sell price. It uses real observed mean/min/max prices, with separate price tables for Alliance and Horde (no neutral AH support). Listing behavior is fully configurable per item class and quality. Unlike other auction managers, it runs very fast and can manage very large auction houses with no lag during scans.

## How it works

Every hour (fixed, not configurable), AuctionSim scans both auction houses:

- **Listing**: it keeps each item class/quality bucket about as full as a real auction house was observed to be in the scan data -- topping a bucket up only once it drops below the observed lower quartile, and choosing which items fill it weighted by how often each was really listed. A per-bucket multiplier in the config scales that target up or down. New listings get a random quantity and a buyout price rolled around the item's known mean price.
- **Buying**: for each auction it doesn't already own, if the price is at or under the item's known mean, it's always queued to buy. If the price is above mean but still under the item's known maximum, it's queued with some probability (randomized each scan, to mimic natural demand variance between real players) rather than always or never. Queued purchases execute within 45 minutes of being queued.
- **Random suffixes**: an item with random properties or suffixes ("of the Bear") has a row per suffix in the scan data, but every suffix is listed and bought at one price per auction house: the median of the rows' market prices (the lower one of the middle two for an even count), with a buy ceiling and listing range scaled from it by the rows' median ratio of each to their own market price. A suffix's own row turned out to be a poor guide to that suffix's price: it predicts the same suffix's row in the other faction's house worse than the item's median does, even when both rows are well sampled.
- Optional `MaxRequiredLevel`/`MaxItemLevel` caps stop it from listing gear above your realm's level, for progression servers running below the max level.

## Installation

1. From your AzerothCore `modules` directory:
   ```
   git clone https://github.com/Moloch17/mod-auctionsim.git
   ```
2. Rebuild AzerothCore.

**Notes:**
- If you've previously used ah-bot or ah-bot-plus, there's no conflict, but the two cannot run at the same time -- disable other auction house bot modules before enabling this one.
- Not intended for use with combined auction house enabled.
- AuctionSim is disabled by default.

## Configuring the Module Using the Companion Addon

This module can be fully configured using a companion interface addon. Copy the ahsim folder in interface_addon into your game's Interface/Addons directory. Once logged in with an account that has GM privileges, run /auctionsim or /ahsim to open the configuration UI. Click the help button for step by step instructions for configuring the module. The same help text in the help menu is reproduced below:

```
AuctionSim - First-Time Setup
=============================

AuctionSim runs a bot character that lists and buys items on the Auction House 
so a low-population realm still has a busy AH.

Everything in this window is saved to auctionsim.conf. You do not need to edit
that file by hand.


1. Requirements
---------------
- A character for the bot to use. A dedicated character on its own account is
  best, but any character works - including the one you are logged in on right
  now.
- Two-side auction interaction must be OFF in worldserver.conf:
      AllowTwoSide.Interaction.Auction = 0
  The module refuses to start otherwise.
- You must be a GM to open this window (/auctionsim or /ahsim).


2. Point the module at the bot character
----------------------------------------
- Click "Set Bot Char".
- Type the character's name and click Okay.
- The server looks the character up, writes its character id and account id to
  auctionsim.conf, and - if the module is already enabled - switches the running
  bot to it right away. No restart needed.
- Success, or the reason it failed, shows in the Results box.


3. Enable the module
--------------------
- Tick "Enabled". It saves immediately and starts the bot.
- Optionally tick "Startup Scan" to run a scan automatically on every server
  start.


4. Choose what the bot lists
----------------------------
In the "Listing Multipliers" grid on the right:

- Max Required Level / Max Item Level: the bot skips items above these. 0 means
  no limit.
- The bot works out how full to keep each category and quality from real auction
  scan data. Each grid cell scales that target: 1 matches the real market, 1.5
  keeps it 50% fuller, 0.5 half as full, 0 disables that cell. Enter a plain
  number like 1, 1.5 or 0.25.
- The scan data was taken from a realm with a saturated auction house, so the
  default values are scaled down.
- When editing a cell you must press enter to apply the change.
- Click "Apply" to send your changes, then "Save To File" to write them to
  auctionsim.conf.
- "Refresh" reloads the values from the server and discards unsaved edits.

5. First run
------------
- Click "Scan". The bot lists new auctions and queues items to buy. Queued buys
  are spread over time, not done all at once.
- Note: Searching the auction house after running a scan while logged in as the
  bot character can take a little while for the auction db to update if there are
  a lot of new auctions.
- After that the module scans on its own on a timer.


Button reference
----------------
Scan            List new auctions and queue buys now.
Delete          Remove every auction the bot currently has listed.
Show Queue      Show the buy queue size and when the next and last buy are due.
Clean Over Cap  Remove bot auctions that are now above the level caps.
Run Tests       Run the module's built-in self-tests; output goes to Results.
Set Bot Char    Choose which character the bot uses (see step 2).
Help            This window.


Notes
-----
- Mail the bot would get from its own auctions is discarded automatically.
- Everything set here is written to auctionsim.conf, so it survives a restart.
```

<img src="images/addon.png">

## GM Commands

All commands need an administrator account (GM level 3) and also work from the worldserver console and over SOAP.

- `.auctionsim scan` -- scan both auction houses now (list new items, queue purchases).
- `.auctionsim showqueue` -- size and timing of the buy queue.
- `.auctionsim delete` -- remove all of the bot's auctions.
- `.auctionsim cleanovercap` -- remove the bot's auctions above the configured level caps.
- `.auctionsim test` -- run the self-tests (briefly lists and buys a few real auctions).
- `.auctionsim price <item id or link>` -- what the bot pays for an item, per auction house.

### `.auctionsim price`

Tells a seller (or a tool) at what price the bot buys an item. It reads only the loaded price data, so it works while the module is disabled.

```
> .auctionsim price 36908
format: 1
item: 36908
name: Frost Lotus
quality: 2
enabled: yes
buyable: yes
vendor_cap: 0
house: name=alliance id=2 data=yes rows=1 samples=22237 market=125000 ceiling=146750 sure=125000 half=135875 tenth=146750
house: name=horde id=6 data=yes rows=1 samples=26518 market=129999 ceiling=159400 sure=129999 half=144699 tenth=159400
```

- All prices are per unit, in copper. The bot compares an auction's buyout divided by its stack size.
- `enabled`: whether the bot is running (it scans and buys only then). `buyable`: `no` for poor-quality (grey) items, which the bot never buys.
- `vendor_cap`: the item's vendor purchase price when a vendor stocks it (`npc_vendor`) in unlimited quantity for gold, else 0. The bot never pays more than that per unit. A vendor that sells it only in limited stock or for tokens doesn't count (issue #6).
- One `house:` line per auction house, as space-separated `key=value` fields. With `data=no` the item isn't in the price data for that house, and the bot never buys it there. With `data=yes`: `market` (outlier-trimmed median) and `ceiling` (75th percentile) of the scanned prices, `rows` and `samples` the price rows and scanned buyouts they come from (an item with random suffixes has a row per suffix and one price for every suffix, the median over its rows: see "Random suffixes" above; any other item has 1 row), then the highest price per unit for each chance of a sale to the bot:
  - `sure`: at or under it the bot always buys: its next hourly scan queues the purchase, which goes through within 45 minutes.
  - `half`: above `sure` and at or under `half`, a 50% chance over the auction's remaining time at each scan (the bot reconsiders it at every scan, so the chances compound: 88% over a 12 h listing, 93% over 24 h, 95% over 48 h).
  - `tenth`: above `half` and at or under `tenth`, at least 10% over the remaining time at each scan (at least 28%, 33% and 37% over 12, 24 and 48 h; each scan rolls where the 50% band ends, between half and 70% of the way to the ceiling). Above `tenth` the bot never buys.
  All three are capped at `vendor_cap` when that isn't 0.
- `format` is currently 1 and changes only if a line changes meaning. Keys and fields may be added; tools should ignore those they don't know.
- An unknown item, or price data that failed to load, is an error.
