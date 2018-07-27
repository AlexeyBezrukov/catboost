#pragma once

#include <library/binsaver/bin_saver.h>

#include <util/generic/vector.h>
#include <util/system/types.h>

struct TAllFeatures {
    TVector<TVector<ui8>> FloatHistograms; // [featureIdx][doc]
    // FloatHistograms[featureIdx] might be empty if feature is const.
    TVector<TVector<int>> CatFeaturesRemapped; // [featureIdx][doc]
    TVector<TVector<int>> OneHotValues; // [featureIdx][valueIdx]
    TVector<bool> IsOneHot;
    size_t GetDocCount() const;
    SAVELOAD(FloatHistograms, CatFeaturesRemapped, OneHotValues, IsOneHot);
};

inline int GetDocCount(const TAllFeatures& allFeatures) {
    return static_cast<int>(allFeatures.GetDocCount());
}