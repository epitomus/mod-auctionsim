# AuctionSim: A Module For AzerothCore WoTLK 3.3.5a

AuctionSim populates and maintains your realm's auction house using price data scraped from a live WoW 3.3.5a economy (Lordaeron, Warmane). Unlike the classic ah-bot module, it isn't limited to a handful of item categories and doesn't price items based on their vendor sell price. It uses real observed mean/min/max prices, with separate price tables for Alliance and Horde (no neutral AH support). Listing behavior is fully configurable per item class and quality. Unlike other auction managers, it runs very fast and can manage very large auction houses with no lag during scans.

## How it works

Every 30 minutes (fixed, not configurable), AuctionSim scans both auction houses:

- **Listing**: it keeps each item class/quality bucket about as full as a real auction house was observed to be in the scan data -- topping a bucket up only once it drops below the observed lower quartile, and choosing which items fill it weighted by how often each was really listed. A per-bucket multiplier in the config scales that target up or down. New listings get a random quantity, a buyout price rolled around the item's known mean price, and a lower starting bid rolled from the starting-bid-to-buyout ratios seen in the real scan data.
- **Buying**: for each auction it doesn't already own, if the price is at or under the item's known mean, it's always queued to buy. If the price is above mean but still under the item's known maximum, it's queued with some probability (randomized each scan, to mimic natural demand variance between real players) rather than always or never.
- **Bidding**: it bids like one more player. On an auction a real player is bidding on it may outbid them, and on a player's auction nobody has bid on it may open the bidding when the starting bid is a clear bargain. Each auction gets its own limit, rolled once between the item's lower-quartile and market price, so the bot wins some bid wars and walks away from others. It never bids at or above the cheapest buyout of the same item on the auction house, the auction's own buyout, or the vendor price. Bids are the game's minimum increment, often rounded up to a clean amount the way a player types it. It never bids in an auction's last 30 minutes, so a player it outbids always has time to answer, and it never removes an auction someone has bid on, so the bidder's gold is never stranded.
- **Random suffixes**: an item with random properties or suffixes ("of the Bear") has a row per suffix in the scan data, but every suffix is listed, bought and bid on at one price per auction house: the median of the rows' market prices (the lower one of the middle two for an even count), with a buy ceiling, listing range and the low end of the bid limit's range scaled from it by the rows' median ratio of each to their own market price. A suffix's own row turned out to be a poor guide to that suffix's price: it predicts the same suffix's row in the other faction's house worse than the item's median does, even when both rows are well sampled.
- Queued buys and bids execute within 20 minutes of being queued, spread out over time. "Run Queue" in the addon (or `.auctionsim runqueue`) forces them all through immediately.
- Optional `MaxRequiredLevel`/`MaxItemLevel` caps stop it from listing gear above your realm's level, for progression servers running below the max level.

## Installation

1. From your AzerothCore `modules` directory:
   ```
   git clone https://github.com/Moloch17/mod-auctionsim.git
   ```
2. Rebuild AzerothCore.
3. Copy `interface_addon/ahsim` into your game's `Interface/AddOns` directory (see below). The module and addon ship as a pair: after updating one, update the other too, or a GM gets a version-mismatch notice.

**Notes:**
- If you've previously used ah-bot or ah-bot-plus, there's no conflict, but the two cannot run at the same time -- disable other auction house bot modules before enabling this one.
- Not intended for use with combined auction house enabled.
- AuctionSim is disabled by default.

## Configuring the Module Using the Companion Addon

This module can be fully configured using a companion interface addon. Copy the ahsim folder in interface_addon into your game's Interface/Addons directory. Once logged in with an account that has GM privileges, run /auctionsim or /ahsim to open the configuration UI. Click the help button for step by step instructions for configuring the module. The same help text is reproduced below:

AuctionSim runs a bot character that lists and buys items on the auction house, so a low-population realm still has
a busy AH.

Everything in this window is saved to `auctionsim.conf`. You do not need to edit that file by hand.

### 1. Requirements

- A character for the bot to use. A dedicated character on its own account is best, but any character works,
  including the one you are logged in on right now.
- Two-side auction interaction must be off in `worldserver.conf`: `AllowTwoSide.Interaction.Auction = 0`. The module
  refuses to start otherwise.
- You must be a GM to open this window (`/auctionsim` or `/ahsim`).

### 2. Point the module at the bot character

- Click **Set Bot Char**.
- Type the character's name and click **Okay**.
- The server looks the character up and writes its character id and account id to `auctionsim.conf`. If the module
  is already enabled, the running bot switches to it right away. No restart needed.
- Success, or the reason it failed, shows in the **Results** box.

### 3. Enable the module

- Tick **Enabled**. It saves immediately and starts the bot.
- Optionally tick **Startup Scan** to run a scan automatically every time the server starts.

### 4. Choose what the bot lists

These settings are on the right of the **Main** tab.

- **Max Required Level** and **Max Item Level**: the bot skips items above these. 0 means no limit.
- **Listing Multipliers**: the bot works out how full to keep each category and quality from real auction scan data.
  Each cell scales that target: 1 matches the real market, 1.5 keeps it 50% fuller, 0.5 half as full, 0 turns the
  cell off. Enter a plain number such as 1, 1.5 or 0.25, and press Enter to apply each cell.
- The scan data comes from a realm with a very full auction house, so the default values are scaled down.
- Click **Apply** to send your changes, then **Save To File** to write them to `auctionsim.conf`.
- **Refresh** reloads the values from the server and discards unsaved edits.

### 5. First run

- Click **Scan**. The bot lists new auctions, each with a buyout and a lower starting bid, and queues actions: items
  to buy outright, plus bids on players' auctions it values (never in an auction's last 30 minutes).
- Queued actions are spread over time rather than done all at once. Click **Run Queue** to force them all through now.
- After a scan, searching the auction house while logged in as the bot character can take a while to update if many
  auctions were added.
- From then on the module scans by itself every 30 minutes.

### 6. Experimental Features tab

The **EXPERIMENTAL FEATURES** tab at the bottom of the window holds features that work but may still change between
releases:

- **Market Mode**: named seller bots and buyers learned from a real market, instead of the Replay bot.
- Its settings, **Market Bots** and **Market Scale**, and its commands: **Market Status**, **Fill**, **Reload** and
  **Purge**.
- **Replay Bidding**: whether the Replay bot bids. Greyed out while Market Mode is ticked.

Ticking or unticking Market Mode offers to restart the worldserver, since a mode change needs a restart. The tab's own
**Help** button explains every control in detail.

### Buttons on the Main tab

- **Scan**: list new auctions and queue buys and bids now. In Market mode, runs one market step.
- **Delete**: remove every bot auction nobody has bid on, market sellers' included. Auctions with a bid are left to
  expire, so the bidder's gold is never stranded.
- **Show Queue**: show the queue size and when the next and last actions are due.
- **Run Queue**: carry out every queued buy and bid right now.
- **Clean Over Cap**: remove bot auctions now above the level caps, again skipping any that have a bid.
- **Run Tests**: run the module's built-in self-tests. Output goes to the Results box.
- **Set Bot Char**: choose which character the bot uses (see step 2).
- **Help**: this window.

# Experimental features

Market mode and Replay Bidding are on the companion addon's **EXPERIMENTAL FEATURES** tab (at the bottom of the window), whose Help button shows this text. The technical details are in [Market mode internals](#market-mode-internals) below.

These features work, but their behaviour may still change between releases.

### Market mode

By default the module runs in Replay mode: one bot lists items from a table of real prices, follows the auction
scans and buys what is cheap. Market mode replaces that with a market learned from Warmane's Lordaeron realm.

- **Named sellers.** Each faction gets its own seller bots, and their names show as the seller in the auction house.
  The module creates their characters itself the first time Market mode starts, on accounts `AHSIMMKTA01`,
  `AHSIMMKTA02` and so on (Alliance) and `AHSIMMKTH01` and so on (Horde), ten characters per account. Nobody can log
  into them. Names already taken on your realm are skipped.
- **What each seller posts.** Each seller has its own list of items, taken from what real sellers of its kind posted
  on Lordaeron, with a posting rate for each item: Trade Goods for some sellers, glyphs or gear for others. Every 30
  minutes the module rolls, item by item, whether the seller posts it this time, so a busy item may go up several
  times a day and a rare one once a week. One item is usually spread over one or two sellers.
- **Prices.** A post is priced against the cheapest listing of the same item already up, players' listings included.
  It is never below the vendor price, and crafted goods are never below what their reagents cost. Crafted gear and
  bags carry the seller's name as their maker once bought. Durations, deposits and expiry are the game's own.
- **Buyers.** Buyers arrive for each item at the rates seen on Lordaeron, with a weekday pattern. Each is willing to
  pay up to some price and buys the cheapest listing at or under it, whoever listed it. That is how players sell to
  the market: list at a fair price and a buyer will come. Purchases go through the bot character set with **Set Bot
  Char** on the Main tab, and are spread over about 20 minutes. Buyers never bid: they buy outright.
- **The market data file.** All of this comes from `auctionsim_market.dat`, which ships with the module and is copied
  next to `auctionsim.conf` when the module is built. If it is missing or from another format version, the module
  refuses to run in Market mode and tells GMs at login.
- **Gold.** The buyer bot is never charged, so gold enters the economy only when it buys a player's listing.
  Everything paid to a seller bot is discarded, so gold players spend on seller listings leaves the economy.

### Settings

#### Market Mode (checkbox)

Switches between Replay (unticked) and Market (ticked). The change is saved at once but only takes effect when the
worldserver restarts. Ticking or unticking it asks **"Restart the worldserver now to apply the change?"**

- **Yes** restarts the server the standard way: a 10 second countdown that players see, like `.server restart 10`.
  The server only comes back by itself if whatever runs it restarts the process (a Docker restart policy, a service
  manager or the restarter script). Refused if a shutdown or restart is already pending.
- **No** leaves it for your next restart and says so in chat: "Market mode cannot be initiated until the worldserver
  is restarted." (or "Replay mode ..." when unticking).

#### Replay Bidding (checkbox)

Whether the Replay bot bids. Ticked (the default), it outbids players who bid on an auction and sometimes opens the
bidding on a player's auction, as a player would. Unticked, it never bids: it queues no new bids and drops the bids it
had already queued. Buying outright is unaffected. Takes effect at once, from the next scan.

It only matters in Replay mode, because Market mode never bids. While Market Mode is ticked the checkbox is greyed out
and shown unchecked, since nothing bids then; its saved value is kept and shown again when you switch back, and the
server refuses to change it.

#### Market Bots (box)

How many named sellers each faction gets (default 100), taken from the names in the data file; fewer if names are
taken. The market's size doesn't depend on it, only how many names the posts are spread over. Applies at restart or
with **Market Reload**.

#### Market Scale (box)

How big the market is, as a fraction of Lordaeron's: the default 0.1 is about 6,000 auctions per faction, and 1 is
about 60,000. It scales both posting and buying. Your realm's population doesn't matter. Applies from the next market
step.

### Commands

#### Market Status

Button, or `.auctionsim market status`. Shows whether the market is running (and why not if it isn't), the scale,
the sellers in use per faction, auctions still waiting to be posted, the buy queue, the last step's numbers per auction
house, and fill progress.

#### Market Fill

Button, or `.auctionsim market fill [alliance|horde]`. Fills the auction house at once instead of waiting a day or
two.

- It simulates the last 48 hours of the market, with the auctions already up as competition, and posts what the
  sellers would have up now, each with its remaining time.
- It never adds more than a full house minus what the sellers already have up, so a fill on a full house adds almost
  nothing.
- Auctions appear at up to 100 per server tick. Both factions by default; the chat command takes `alliance` or
  `horde` for one.
- Refused while the market isn't running, its sellers are still being set up, or a fill is already running.

#### Market Reload

Button, or `.auctionsim market reload`. Re-reads `auctionsim.conf` (Market Bots, Market Scale, Replay Bidding) and
`auctionsim_market.dat` and restarts the market with them, without restarting the server.

- Only works when the server started in Market mode; switching modes needs a restart.
- The buyer bot is reloaded too, which empties its buy queue.

#### Market Purge

Button, or `.auctionsim market purge [confirm]`. Removes everything Market mode created.

- The first click only reports what would be deleted: the seller accounts and characters, their auctions (and how
  many have bids) and their mail. If there is anything, a second popup asks you to confirm.
- Confirmed (`.auctionsim market purge confirm`), it:
  - stops the market until a restart or Market Reload, and drops the queued market purchases;
  - removes every seller auction; anyone who bid on one gets the bid back by mail, as when an auction is cancelled;
  - deletes the seller characters and the `AHSIMMKT` accounts the standard way.
- It refuses entirely, deleting nothing, if a seller account holds a character the module didn't create (levelled,
  played, wrong race or class, online) or has GM access. It never touches the buyer bot.
- With Market Mode still ticked, the next restart creates the sellers again. Untick it first if you want them gone
  for good. Running it again finds nothing and says so.

#### Help

This window.

## Market mode internals

`AuctionSim.Mode = Market` (default `Replay`, the behaviour above) runs a different market, learned from Lordaeron's scans and shipped as `data/auctionsim_market.dat` (format: `ml/MARKET_FORMAT.md`; produced by `ml/s6_export.py`):

- **Named sellers.** Each faction gets `AuctionSim.Market.Bots` (default 100) seller bots whose names show as the seller in the AH. Their names come from the data file; names already taken on the realm are skipped. The module creates their characters on first start: real character rows on accounts `AHSIMMKTA01`, `AHSIMMKTA02`, ... (Alliance races) and `AHSIMMKTH01`, ... (Horde races), ten characters per account, each account with a random password nobody is told (a login on a seller is kicked anyway). They never log in, survive restarts, and their names can't be taken by players.
- **Posts.** Every 30 minutes each seller's basket posts per the learned rates, stacks and prices: priced against the cheapest listing of the item already up (any owner, players included), never under the vendor price or, for crafted goods, the reagents' cost. Durations, deposits and expiry are the core's.
- **Buyers.** Buyers arrive per item at learned rates (with a weekday pattern), each with a reservation price, and buy the cheapest listing at or under it -- whoever owns it, so players sell to the market by listing at a fair price. Purchases go through the bot character (`BotCharacterID`) and its queue, spread over 20 minutes. A player's listing of a vendor-stocked item is never bought above the vendor price.
- **Gold.** The only gold that enters the economy is the price the buyer bot pays for a *player's* listing (the core's normal sale mail, minus its cut). The buyer itself is never debited. Everything paid to a seller bot -- sale proceeds, returned deposits, expired items -- is discarded, so gold players spend on bot listings leaves the economy. Only the auction house's mail is discarded: mail a player or GM sends to a bot character is delivered as usual and goes back to its sender when it expires. Seller bots hold no gold; their deposits are computed as the core would and recorded on the auction, not debited.
- `AuctionSim.Market.Scale` (default 0.1) sizes the market as a fraction of Lordaeron's (~60k auctions per faction at 1). It doesn't depend on the realm's population.
- `.auctionsim market fill [alliance|horde]` populates a house at once (MARKET_FORMAT.md's Fill): it simulates the 48 hours before now on virtual seller listings, with the real house as competitors that are never bought, and takes the survivors as the house's steady state: it adds at most that total minus what the sellers already have up (an item's excess over the sellers' own listings of it, items in random order), so a fill on an empty house adds a full house and one on a full house almost nothing. Each is created with its remaining time through the same 100-per-tick creation queue. The simulation itself is spread over ticks (at most ~4 ms each).
- `.auctionsim market purge` reports what it would delete; `.auctionsim market purge confirm` deletes the module's seller accounts (`AHSIMMKTA..`/`AHSIMMKTH..`) and their characters through the core's `AccountMgr::DeleteAccount`, after removing every seller auction (a bidder gets the bid back by the core's cancel mail). It refuses entirely if a seller account holds a character the module didn't create (levelled, played, wrong race/class/faction, online, GM access, or the buyer bot), and never touches the buyer bot. The market then stops until restart or `.auctionsim market reload`; with `Mode = Market` that restart creates the sellers again, so set `Mode = Replay` (or disable the module) first to remove the footprint.
- `.auctionsim market status` shows the sellers in use, the last step's numbers and the work still queued; `.auctionsim market reload` re-reads `auctionsim.conf` and the data file. Without a current `auctionsim_market.dat` the module refuses to run in Market mode and tells GMs at login.
- Cost: one pass over each house per 30 minutes; at Scale 0.1 a step's arithmetic takes about 0.5 ms per house, and auctions are created at most 100 per world tick.


<img src="images/addon.png">

## GM Commands

All commands need an administrator account (GM level 3) and also work from the worldserver console and over SOAP.

- `.auctionsim scan` -- scan both auction houses now (list new items, queue buys and bids; in Market mode, one market step).
- `.auctionsim showqueue` -- size and timing of the queue.
- `.auctionsim runqueue` -- carry out every queued buy and bid now.
- `.auctionsim delete` -- remove the bot's auctions nobody has bid on.
- `.auctionsim cleanovercap` -- remove the bot's auctions above the configured level caps, again skipping any with a bid.
- `.auctionsim test` -- run the self-tests (briefly lists and buys a few real auctions).
- `.auctionsim market status|fill|reload|purge` -- Market mode, see "Experimental features" above.
- `.auctionsim price <item id or link>` -- what the bot pays for an item, per auction house.

### `.auctionsim price`

Tells a seller (or a tool) at what price the Replay bot buys an item outright. It reads only the loaded price data, so it works while the module is disabled.

```
> .auctionsim price 36908
format: 1
item: 36908
name: Frost Lotus
quality: 2
mode: Replay
enabled: yes
buyable: yes
vendor_cap: 0
house: name=alliance id=2 data=yes rows=1 samples=22331 market=124500 ceiling=145250 sure=124500 half=134875 tenth=145250
house: name=horde id=6 data=yes rows=1 samples=26690 market=129999 ceiling=159000 sure=129999 half=144499 tenth=159000
```

- All prices are per unit, in copper. The bot compares an auction's buyout divided by its stack size.
- `mode`: `Replay` or `Market` (AuctionSim.Mode). The prices describe Replay mode's buying; Market mode's buyers buy by other rules.
- `enabled`: whether the bot is running (it scans and buys only then). `buyable`: `no` for poor-quality (grey) items, which the bot never buys.
- `vendor_cap`: the item's vendor purchase price when a vendor stocks it (`npc_vendor`) in unlimited quantity for gold, else 0. The bot never pays more than that per unit. A vendor that sells it only in limited stock or for tokens doesn't count (issue #6).
- One `house:` line per auction house, as space-separated `key=value` fields. With `data=no` the item isn't in the price data for that house, and the bot never buys it there. With `data=yes`: `market` (outlier-trimmed median) and `ceiling` (75th percentile) of the scanned prices, `rows` and `samples` the price rows and scanned buyouts they come from (an item with random suffixes has a row per suffix and one price for every suffix, pooled over its rows: see "Random suffixes" above; any other item has 1 row), then the highest price per unit for each chance of a sale to the bot:
  - `sure`: at or under it the bot always buys: its next scan queues the purchase, which goes through within 20 minutes.
  - `half`: above `sure` and at or under `half`, a 50% chance over the auction's remaining time at each scan (the bot reconsiders it at every scan, every 30 minutes, so the chances compound: 93% over a 12 h listing, 95% over 24 h, 97% over 48 h).
  - `tenth`: above `half` and at or under `tenth`, at least 10% over the remaining time at each scan (at least 33%, 37% and 42% over 12, 24 and 48 h; each scan rolls where the 50% band ends, between half and 70% of the way to the ceiling). Above `tenth` the bot never buys.
  All three are capped at `vendor_cap` when that isn't 0. They are buyouts: an auction whose starting bid is below its buyout can also be won by the bot's bidding (see "Bidding" above).
- `format` is currently 1 and changes only if a line changes meaning. Keys and fields may be added; tools should ignore those they don't know.
- An unknown item, or price data that failed to load, is an error.
