#pragma once

#include "enums.h"
#include "option.h"

#include <util/generic/map.h>
#include <util/system/types.h>


namespace NJson {
    class TJsonValue;
}

ELossFunction ParseLossType(TStringBuf lossDescription);

TMap<TString, TString> ParseLossParams(TStringBuf lossDescription);

namespace NCatboostOptions {
    const int MAX_AUTOGENERATED_PAIRS_COUNT = Max<int>();

    class TLossDescription {
    public:
        explicit TLossDescription();

        ELossFunction GetLossFunction() const;

        void Load(const NJson::TJsonValue& options);
        void Save(NJson::TJsonValue* options) const;

        bool operator==(const TLossDescription& rhs) const;
        bool operator!=(const TLossDescription& rhs) const;

        const TMap<TString, TString>& GetLossParams() const;

    public:
        TOption<ELossFunction> LossFunction;
        TOption<TMap<TString, TString>> LossParams;
    };

    double GetLogLossBorder(const TLossDescription& lossFunctionConfig);

    double GetAlpha(const TMap<TString, TString>& lossParams);
    double GetAlpha(const TLossDescription& lossFunctionConfig);

    double GetAlphaQueryCrossEntropy(const TMap<TString, TString>& lossParams);
    double GetAlphaQueryCrossEntropy(const TLossDescription& lossFunctionConfig);

    int GetYetiRankPermutations(const TLossDescription& lossFunctionConfig);

    ESamplingType GetYetiRankSamplingType(const TLossDescription& lossFunctionConfig);

    double GetYetiRankDecay(const TLossDescription& lossFunctionConfig);

    double GetLqParam(const TLossDescription& lossFunctionConfig);

    double GetQuerySoftMaxLambdaReg(const TLossDescription& lossFunctionConfig);

    ui32 GetMaxPairCount(const TLossDescription& lossFunctionConfig);

    TLossDescription ParseLossDescription(TStringBuf stringLossDescription);
}

void ValidateHints(const TMap<TString, TString>& hints);

TMap<TString, TString> ParseHintsDescription(TStringBuf hintsDescription);
TString MakeHintsDescription(const TMap<TString, TString>& hints);

NJson::TJsonValue LossDescriptionToJson(TStringBuf lossDescription);
