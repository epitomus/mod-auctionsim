#include <chrono>
#include <utility>
#include "AuctionPricing.h"
#include "AuctionSim.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

namespace
{
    // Shared "is the module usable?" gate for every subcommand. Replies to the GM
    // and returns false when it isn't.
    bool RequireEnabled(ChatHandler* handler)
    {
        if (AuctionSim::instance() && AuctionSim::instance()->isEnabled)
        {
            return true;
        }
        handler->SendSysMessage("AuctionSim module is disabled.");
        return false;
    }

    // Runs `fn` and returns how long it took, in whole milliseconds.
    template <typename Fn>
    long long TimedMs(Fn&& fn)
    {
        auto start = std::chrono::steady_clock::now();
        std::forward<Fn>(fn)();
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
}

class AuctionSimCommandScript : public CommandScript
{
public:
    AuctionSimCommandScript() : CommandScript("AuctionSimCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable auctionSimSubCommandTable = {
            {"scan", HandleScanAuctionsCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"delete", HandleDeleteAuctionsCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"test", HandleTestCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"cleanovercap", HandleCleanOverCapCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"showqueue", HandleShowQueueCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"price", HandlePriceCommand, SEC_ADMINISTRATOR, Console::Yes},
        };
        static ChatCommandTable commandTable = {
            {"auctionsim", auctionSimSubCommandTable},
        };
        return commandTable;
    }

    static bool HandleScanAuctionsCommand(ChatHandler* handler)
    {
        if (!RequireEnabled(handler))
        {
            return true;
        }

        size_t queueSizeBefore = AuctionSim::instance()->GetBuyQueue().size();
        long long elapsed = TimedMs([] {
            AuctionSim::instance()->ScanAuctions(AuctionHouseId::Alliance);
            AuctionSim::instance()->ScanAuctions(AuctionHouseId::Horde);
        });
        size_t queueSizeAfter = AuctionSim::instance()->GetBuyQueue().size();

        std::string message = fmt::format(
            "Auction scan completed in {} ms. Added {} item(s) to buy queue ({} total).",
            elapsed,
            queueSizeAfter - queueSizeBefore,
            queueSizeAfter);
        LOG_INFO("module", "{}", message);
        handler->SendSysMessage(message);
        return true;
    }

    static bool HandleDeleteAuctionsCommand(ChatHandler* handler)
    {
        if (!RequireEnabled(handler))
        {
            return true;
        }

        long long elapsed = TimedMs([] { AuctionSim::instance()->DeleteAuctions(); });
        handler->SendSysMessage(fmt::format("Auction delete completed in {} ms", elapsed));
        return true;
    }

    static bool HandleTestCommand(ChatHandler* handler)
    {
        if (!RequireEnabled(handler))
        {
            return true;
        }

        auto results = AuctionSim::instance()->RunTests();

        uint32 passed = 0;
        for (auto const& result : results)
        {
            if (result.passed)
            {
                passed++;
                LOG_INFO("module", "AuctionSim test [PASS] {}: {}", result.name, result.detail);
                handler->SendSysMessage(fmt::format("[PASS] {}: {}", result.name, result.detail));
            }
            else
            {
                LOG_ERROR("module", "AuctionSim test [FAIL] {}: {}", result.name, result.detail);
                handler->SendSysMessage(fmt::format("[FAIL] {}: {}", result.name, result.detail));
            }
        }

        std::string summary = fmt::format("AuctionSim test suite: {}/{} passed", passed, results.size());
        LOG_INFO("module", "{}", summary);
        handler->SendSysMessage(summary);
        return true;
    }

    static bool HandleCleanOverCapCommand(ChatHandler* handler)
    {
        if (!RequireEnabled(handler))
        {
            return true;
        }

        uint32 removedCount = 0;
        long long elapsed = TimedMs([&] { removedCount = AuctionSim::instance()->CleanOverCapAuctions(); });
        handler->SendSysMessage(
            fmt::format("Removed {} over-cap auction(s) in {} ms", removedCount, elapsed));
        return true;
    }

    static bool HandleShowQueueCommand(ChatHandler* handler)
    {
        if (!RequireEnabled(handler))
        {
            return true;
        }

        AuctionSim::BuyQueueStatus status =
            AuctionSim::instance()->GetBuyQueueStatus(GameTime::GetGameTime().count());

        std::string message = status.size == 0
            ? std::string("AuctionSim buy queue is empty.")
            : fmt::format(
                  "AuctionSim buy queue: {} item(s) | next buy in {}s | last buy in {}s",
                  status.size,
                  status.nextBuyInSeconds,
                  status.lastBuyInSeconds);
        LOG_INFO("module", "{}", message);
        handler->SendSysMessage(message);
        return true;
    }

    // What the bot pays for an item, per auction house, as `key: value` lines for
    // tools (README: "GM commands"). Reads the loaded price data only, so it works
    // while the module is disabled; `enabled` says whether the bot is buying.
    static bool HandlePriceCommand(ChatHandler* handler, Variant<Hyperlink<item>, uint32> itemArg)
    {
        uint32 itemId = itemArg.holds_alternative<Hyperlink<item>>()
            ? itemArg.get<Hyperlink<item>>()->Item->ItemId
            : itemArg.get<uint32>();

        AuctionSim* sim = AuctionSim::instance();
        ASConfig const* config = sim ? sim->GetConfig() : nullptr;
        if (!config)
        {
            handler->SendErrorMessage("AuctionSim price data (auctionsim.dat) is not loaded.");
            return false;
        }

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
        {
            handler->SendErrorMessage(fmt::format("Item {} does not exist.", itemId), false);
            return false;
        }

        uint32 vendorCap = config->VendorBuyCap(proto);
        handler->SendSysMessage("format: 1");
        handler->SendSysMessage(fmt::format("item: {}", proto->ItemId));
        handler->SendSysMessage(fmt::format("name: {}", proto->Name1));
        handler->SendSysMessage(fmt::format("quality: {}", proto->Quality));
        handler->SendSysMessage(fmt::format("enabled: {}", sim->isEnabled && sim->GetBotPlayer() ? "yes" : "no"));
        handler->SendSysMessage(
            fmt::format("buyable: {}", AuctionPricing::IsBuyableQuality(proto->Quality) ? "yes" : "no"));
        handler->SendSysMessage(fmt::format("vendor_cap: {}", vendorCap));

        for (auto [houseId, houseName] : {std::pair{AuctionHouseId::Alliance, "alliance"},
                                          std::pair{AuctionHouseId::Horde, "horde"}})
        {
            // The same lookup as ScanAuctions: keyed by item, not suffix, so a
            // random-suffix item gets the first of its rows.
            ScannedItem const* scanned = config->FindScannedItem(houseId, proto->Class, proto->Quality, itemId);
            if (!scanned)
            {
                handler->SendSysMessage(
                    fmt::format("house: name={} id={} data=no", houseName, static_cast<uint32>(houseId)));
                continue;
            }

            uint32 market = scanned->GetMarketPrice();
            uint32 ceiling = scanned->GetBuyCeiling();
            AuctionPricing::BuyPriceTiers tiers = AuctionPricing::CalculateBuyPriceTiers(market, ceiling, vendorCap);
            handler->SendSysMessage(fmt::format(
                "house: name={} id={} data=yes suffix={} samples={} market={} ceiling={} sure={} half={} tenth={}",
                houseName,
                static_cast<uint32>(houseId),
                scanned->GetSuffixID(),
                scanned->GetSampleCount(),
                market,
                ceiling,
                tiers.sure,
                tiers.half,
                tiers.tenth));
        }
        return true;
    }
};

void AddSC_AuctionCommandScript() { new AuctionSimCommandScript(); }
