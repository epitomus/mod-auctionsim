#include "AuctionBuyingService.h"
#include <algorithm>
#include "AuctionPricing.h"
#include "Bot.h"
#include "GameTime.h"
#include "Log.h"
#include "Mail.h"

AuctionBuyingService::AuctionBuyingService(Bot& bot) : _bot(bot) {}

void AuctionBuyingService::RollTolerance() { _tolerance = AuctionPricing::RollBuyTolerance(); }

void AuctionBuyingService::ConsiderForPurchase(
    AuctionEntry* auction, uint32 pricePerItem, uint32 marketPrice, uint32 ceilingPrice)
{
    if (_queuedAuctionIds.count(auction->Id) > 0)
    {
        return;
    }

    time_t now = GameTime::GetGameTime().count();
    uint32 remainingScans = AuctionPricing::CalculateRemainingScans(auction->expire_time - now);

    if (!AuctionPricing::ShouldBuyAtPrice(pricePerItem, marketPrice, ceilingPrice, _tolerance, remainingScans))
    {
        return;
    }

    time_t buyTime = AuctionPricing::RollBuyTime(auction->expire_time, now);
    _queue.push_back({auction->Id, auction->GetHouseId(), buyTime});
    _queuedAuctionIds.insert(auction->Id);
}

void AuctionBuyingService::SortQueue()
{
    // Descending by buyTime so the soonest-due purchase sits at the back.
    std::sort(_queue.begin(), _queue.end(), [](QueuedPurchase const& a, QueuedPurchase const& b) {
        return a.buyTime > b.buyTime;
    });
}

void AuctionBuyingService::ProcessDueQueue()
{
    if (_queue.empty())
    {
        return;
    }

    QueuedPurchase next = _queue.back();
    if (GameTime::GetGameTime().count() < next.buyTime)
    {
        return;
    }

    _queue.pop_back();
    _queuedAuctionIds.erase(next.auctionId);

    AuctionHouseObject* house = sAuctionMgr->GetAuctionsMapByHouseId(next.houseId);
    if (!house)
    {
        return;
    }

    AuctionEntry* auction = house->GetAuction(next.auctionId);
    if (!auction)
    {
        return;
    }

    BuyItem(auction, next.houseId);
}

void AuctionBuyingService::EnqueueForTest(AuctionEntry* auction, time_t buyTime)
{
    _queue.push_back({auction->Id, auction->GetHouseId(), buyTime});
    _queuedAuctionIds.insert(auction->Id);
}

void AuctionBuyingService::BuyItem(AuctionEntry* auction, AuctionHouseId houseId)
{
    // A bid-only auction has nothing to buy out; "buying" it at 0 would destroy the
    // seller's item for nothing. ScanAuctions never queues one, but the queue is
    // filled up to an hour earlier, so check again.
    if (auction->buyout == 0)
    {
        return;
    }

    auto trans = CharacterDatabase.BeginTransaction();

    // Buying out an auction someone has bid on: refund their bid, as the core does
    // for a player's buyout (WorldSession::HandleAuctionPlaceBid).
    Player* botPlayer = &_bot.GetPlayerRef();
    if (auction->bidder && auction->bidder != botPlayer->GetGUID())
    {
        sAuctionMgr->SendAuctionOutbiddedMail(auction, auction->buyout, botPlayer, trans);
    }

    auction->bidder = botPlayer->GetGUID();
    auction->bid = auction->buyout;
    sAuctionMgr->SendAuctionSuccessfulMail(auction, trans);
    auction->DeleteFromDB(trans);
    sAuctionMgr->RemoveAItem(auction->item_guid, true, &trans);  // destroy the bought item, don't leak it
    sAuctionMgr->GetAuctionsMapByHouseId(houseId)->RemoveAuction(auction);

    CharacterDatabase.CommitTransaction(trans);
}
