#include "ScannedItem.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include "ASParse.h"
#include "Tokenize.h"

namespace
{
    // First strictly-positive value in `values`, else `fallback`. Encodes the
    // getters' shared "prefer the outlier-trimmed stat, then fall back through the
    // raw ones" rule in one place so the fallback order stays reviewable.
    uint32 FirstPositive(std::initializer_list<uint32> values, uint32 fallback)
    {
        for (uint32 v : values)
        {
            if (v > 0)
            {
                return v;
            }
        }
        return fallback;
    }

    // The lower median of `value(row)` over the rows: with an even count, the lower of
    // the two middle values. `rows` is non-empty.
    template <typename T, typename Value>
    T LowerMedian(std::vector<ScannedItem const*> const& rows, Value value)
    {
        std::vector<T> values;
        values.reserve(rows.size());
        for (ScannedItem const* row : rows)
        {
            values.push_back(value(*row));
        }
        auto mid = values.begin() + (values.size() - 1) / 2;
        std::nth_element(values.begin(), mid, values.end());
        return *mid;
    }

    // `market` scaled by the lower median over the rows of `figure(row) / row's market`,
    // rounded and clamped to uint32. A row with no market price counts as ratio 1.
    template <typename Figure>
    uint32 ScaledByMedianRatio(std::vector<ScannedItem const*> const& rows, uint32 market, Figure figure)
    {
        double ratio = LowerMedian<double>(rows, [&figure](ScannedItem const& row) {
            uint32 rowMarket = row.GetMarketPrice();
            return rowMarket > 0 ? static_cast<double>(figure(row)) / rowMarket : 1.0;
        });
        double scaled = std::round(static_cast<double>(market) * ratio);
        return static_cast<uint32>(std::clamp(scaled, 0.0, static_cast<double>(UINT32_MAX)));
    }
}

bool ParseStatBlock(std::vector<std::string_view> const& fields, size_t offset, StatBlock& out)
{
    if (offset + ScannedItem::kStatsPerBlock > fields.size())
    {
        return false;
    }

    // Order matches StatBlock's declaration and compile-data.cpp's emitStats().
    uint32* const cols[ScannedItem::kStatsPerBlock] = {
        &out.low, &out.high, &out.mean, &out.median, &out.mode,
        &out.q1, &out.q3,
        &out.adjLow, &out.adjHigh, &out.adjMean, &out.adjMedian, &out.adjMode,
    };
    for (size_t i = 0; i < ScannedItem::kStatsPerBlock; ++i)
    {
        if (!ASParse::ClampedU32(fields[offset + i], *cols[i]))
        {
            return false;
        }
    }
    return true;
}

std::optional<ScannedItem> ScannedItem::TryParse(std::string_view dataLine)
{
    std::vector<std::string_view> f = Acore::Tokenize(dataLine, ':', false);
    if (f.size() != kRowFields)
    {
        return std::nullopt;
    }

    ScannedItem item;

    uint32 factionNum = 0;
    if (!ASParse::Integer(f[0], factionNum) || factionNum > UINT8_MAX)
    {
        return std::nullopt;
    }
    item.factionNum = static_cast<uint8>(factionNum);

    if (!ASParse::Integer(f[1], item.itemID) || !ASParse::Integer(f[2], item.suffixID) ||
        !ASParse::Integer(f[3], item.sampleCount))
    {
        return std::nullopt;
    }

    StatBlock* const blocks[kStatBlockCount] = {&item.price, &item.stack, &item.listing, &item.bidRatio};
    for (size_t b = 0; b < kStatBlockCount; ++b)
    {
        if (!ParseStatBlock(f, kIdentityFields + b * kStatsPerBlock, *blocks[b]))
        {
            return std::nullopt;
        }
    }

    // Two trailing counts: bidRatioSampleCount, then listingSnapshotCount (last).
    if (!ASParse::ClampedU32(f[kRowFields - 2], item.bidRatioSampleCount) ||
        !ASParse::ClampedU32(f[kRowFields - 1], item.listingSnapshotCount))
    {
        return std::nullopt;
    }

    return item;
}

uint32 ScannedItem::GetMarketPrice() const
{
    return FirstPositive({price.adjMedian, price.adjMean, price.median}, price.mean);
}

uint32 ScannedItem::GetListLow() const { return FirstPositive({price.adjLow}, GetMarketPrice()); }
uint32 ScannedItem::GetListHigh() const { return FirstPositive({price.adjHigh}, GetMarketPrice()); }

uint32 ScannedItem::GetBuyCeiling() const
{
    uint32 market = GetMarketPrice();
    return price.q3 > market ? price.q3 : market;
}

uint32 ScannedItem::GetBidValuationLow() const
{
    uint32 market = GetMarketPrice();
    uint32 low = FirstPositive({price.q1}, GetListLow());
    return low < market ? low : market;
}

uint32 ScannedItem::GetTypicalStackSize() const
{
    return FirstPositive({stack.adjMode, stack.adjMedian, stack.mode, stack.median}, 1);
}

uint32 ScannedItem::GetStackLow() const { return FirstPositive({stack.adjLow}, 1); }
uint32 ScannedItem::GetStackHigh() const { return FirstPositive({stack.adjHigh}, GetTypicalStackSize()); }

uint32 ScannedItem::GetTypicalListingCount() const
{
    return FirstPositive({listing.adjMedian, listing.adjMean, listing.median, listing.mean}, 1);
}

uint32 ScannedItem::GetBidRatioTypicalBp() const
{
    // Same "prefer the trimmed stat, fall back through the raw ones" order as the
    // price getters; a fully degenerate bucket means "no bid data" -> ratio 1.0.
    uint32 bp = FirstPositive({bidRatio.adjMedian, bidRatio.adjMean, bidRatio.median, bidRatio.mean}, 0);
    return bp > 0 ? bp : 10000;
}

uint32 ScannedItem::GetBidRatioLowBp() const { return FirstPositive({bidRatio.adjLow}, GetBidRatioTypicalBp()); }
uint32 ScannedItem::GetBidRatioHighBp() const { return FirstPositive({bidRatio.adjHigh}, GetBidRatioTypicalBp()); }

float ScannedItem::GetBidRatioTypical() const { return GetBidRatioTypicalBp() / 10000.0f; }

ScannedItem ScannedItem::Pool(std::vector<ScannedItem const*> const& rows)
{
    uint32 market = LowerMedian<uint32>(rows, [](ScannedItem const& row) { return row.GetMarketPrice(); });
    uint32 ceiling = std::max(
        ScaledByMedianRatio(rows, market, [](ScannedItem const& row) { return row.GetBuyCeiling(); }), market);
    uint32 listLow = ScaledByMedianRatio(rows, market, [](ScannedItem const& row) { return row.GetListLow(); });
    uint32 listHigh = ScaledByMedianRatio(rows, market, [](ScannedItem const& row) { return row.GetListHigh(); });
    uint32 bidLow = std::min(
        ScaledByMedianRatio(rows, market, [](ScannedItem const& row) { return row.GetBidValuationLow(); }), market);

    uint64_t samples = 0;
    for (ScannedItem const* row : rows)
    {
        samples += row->GetSampleCount();
    }

    // Every price stat is set, not only those the getters read first, so no fallback
    // can reach a figure of the first row's.
    ScannedItem pooled = *rows.front();
    pooled.sampleCount = static_cast<uint32>(std::min<uint64_t>(samples, UINT32_MAX));
    pooled.price.low = pooled.price.adjLow = listLow;
    pooled.price.high = pooled.price.adjHigh = listHigh;
    pooled.price.mean = pooled.price.median = pooled.price.mode = market;
    pooled.price.adjMean = pooled.price.adjMedian = pooled.price.adjMode = market;
    pooled.price.q1 = bidLow;
    pooled.price.q3 = ceiling;
    return pooled;
}
