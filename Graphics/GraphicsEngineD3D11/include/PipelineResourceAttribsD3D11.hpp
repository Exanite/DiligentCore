/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

/// \file
/// Declaration of Diligent::PipelineResourceAttribsD3D11 struct

#include <array>

#include "BasicTypes.h"
#include "DebugUtilities.hpp"
#include "HashUtils.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

enum D3D11_RESOURCE_RANGE : Uint32
{
    D3D11_RESOURCE_RANGE_CBV = 0,
    D3D11_RESOURCE_RANGE_SRV,
    D3D11_RESOURCE_RANGE_SAMPLER,
    D3D11_RESOURCE_RANGE_UAV,
    D3D11_RESOURCE_RANGE_COUNT,
    D3D11_RESOURCE_RANGE_UNKNOWN = ~0u
};
D3D11_RESOURCE_RANGE ShaderResourceToDescriptorRange(SHADER_RESOURCE_TYPE Type);


/// Resource binding points in all shader stages.
// sizeof(D3D11ResourceBindPoints) == 8, x64
struct D3D11ResourceBindPoints
{
    /// Number of different shader types (Vertex, Pixel, Geometry, Domain, Hull, Compute)
    static constexpr Uint32 NumShaderTypes = 6;

    D3D11ResourceBindPoints() noexcept
    {
#ifdef DILIGENT_DEBUG
        for (auto BindPoint : Bindings)
            VERIFY_EXPR(BindPoint == InvalidBindPoint);
#endif
    }

    D3D11ResourceBindPoints(const D3D11ResourceBindPoints&) noexcept = default;

    SHADER_TYPE GetActiveStages() const
    {
        return static_cast<SHADER_TYPE>(ActiveStages);
    }

    bool IsEmpty() const
    {
        return GetActiveStages() == SHADER_TYPE_UNKNOWN;
    }

    bool IsStageActive(Uint32 ShaderInd) const
    {
        bool IsActive = (GetActiveStages() & (1u << ShaderInd)) != 0;
        VERIFY_EXPR((IsActive && Bindings[ShaderInd] != InvalidBindPoint ||
                     !IsActive && Bindings[ShaderInd] == InvalidBindPoint));
        return IsActive;
    }

    Uint8 operator[](Uint32 ShaderInd) const
    {
        return Bindings[ShaderInd];
    }

    size_t GetHash() const
    {
        size_t Hash = 0;
        for (auto Binding : Bindings)
            HashCombine(Hash, Binding);
        return Hash;
    }

    bool operator==(const D3D11ResourceBindPoints& rhs) const
    {
        return Bindings == rhs.Bindings;
    }

    D3D11ResourceBindPoints operator+(Uint32 value) const
    {
        D3D11ResourceBindPoints NewBindPoints{*this};
        for (auto Stages = GetActiveStages(); Stages != 0;)
        {
            auto ShaderInd = ExtractFirstShaderStageIndex(Stages);
            VERIFY_EXPR(Uint32{Bindings[ShaderInd]} + value < InvalidBindPoint);
            NewBindPoints.Bindings[ShaderInd] = Bindings[ShaderInd] + static_cast<Uint8>(value);
        }
        return NewBindPoints;
    }

private:
    struct SetBindPointHelper
    {
        SetBindPointHelper(D3D11ResourceBindPoints& _BindPoints,
                           const Uint32             _ShaderInd) :
            BindPoints{_BindPoints},
            ShaderInd{_ShaderInd}
        {}

        Uint8 operator=(Uint32 BindPoint)
        {
            BindPoints.Set(ShaderInd, BindPoint);
            return static_cast<Uint8>(BindPoint);
        }

        operator Uint8() const
        {
            return static_cast<const D3D11ResourceBindPoints&>(BindPoints)[ShaderInd];
        }

    private:
        D3D11ResourceBindPoints& BindPoints;
        const Uint32             ShaderInd;
    };

public:
    SetBindPointHelper operator[](Uint32 ShaderInd)
    {
        return SetBindPointHelper{*this, ShaderInd};
    }

private:
    void Set(Uint32 ShaderInd, Uint32 BindPoint)
    {
        VERIFY_EXPR(ShaderInd < NumShaderTypes);
        VERIFY(BindPoint < InvalidBindPoint, "Bind point (", BindPoint, ") is out of range");

        Bindings[ShaderInd] = static_cast<Uint8>(BindPoint);
        ActiveStages |= Uint32{1} << ShaderInd;
    }

    static constexpr Uint8 InvalidBindPoint = 0xFF;

    //     0      1      2      3      4      5
    // |  PS  |  VS  |  GS  |  HS  |  DS  |  CS  |
    std::array<Uint8, NumShaderTypes> Bindings{InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint};

    Uint8 ActiveStages = 0;
};


/// Resource counters for all shader stages and all resource types
using D3D11ShaderResourceCounters = std::array<std::array<Uint8, D3D11ResourceBindPoints::NumShaderTypes>, D3D11_RESOURCE_RANGE_COUNT>;


// sizeof(PipelineResourceAttribsD3D11) == 12, x64
struct PipelineResourceAttribsD3D11
{
private:
    static constexpr Uint32 _SamplerIndBits      = 10;
    static constexpr Uint32 _SamplerAssignedBits = 1;

public:
    static constexpr Uint32 InvalidSamplerInd = (1u << _SamplerIndBits) - 1;

    // clang-format off
    const Uint32    SamplerInd           : _SamplerIndBits;       // Index of the assigned sampler in m_Desc.Resources.
    const Uint32    ImtblSamplerAssigned : _SamplerAssignedBits;  // Immutable sampler flag.
    D3D11ResourceBindPoints BindPoints;
    // clang-format on

    PipelineResourceAttribsD3D11(Uint32 _SamplerInd,
                                 bool   _ImtblSamplerAssigned) noexcept :
        // clang-format off
            SamplerInd          {_SamplerInd                    },
            ImtblSamplerAssigned{_ImtblSamplerAssigned ? 1u : 0u}
    // clang-format on
    {
        VERIFY(SamplerInd == _SamplerInd, "Sampler index (", _SamplerInd, ") exceeds maximum representable value");
    }

    bool IsSamplerAssigned() const { return SamplerInd != InvalidSamplerInd; }
    bool IsImmutableSamplerAssigned() const { return ImtblSamplerAssigned != 0; }

    bool IsCompatibleWith(const PipelineResourceAttribsD3D11& rhs) const
    {
        // Ignore cache offset and sampler index.
        // clang-format off
        return IsImmutableSamplerAssigned() == rhs.IsImmutableSamplerAssigned() &&
               BindPoints                   == rhs.BindPoints;
        // clang-format on
    }

    size_t GetHash() const
    {
        return ComputeHash(IsImmutableSamplerAssigned(), BindPoints.GetHash());
    }
};

} // namespace Diligent
