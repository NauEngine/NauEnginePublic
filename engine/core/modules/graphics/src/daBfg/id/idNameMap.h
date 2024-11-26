// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#pragma once

#include "nau/utils/dag_oaHashNameMap.h"
#include <EASTL/string_view.h>


template <class EnumType>
struct IdNameMap : dag::OAHashNameMap<false>
{
  using Container = dag::OAHashNameMap<false>;
  using Container::Container;

  EnumType addNameId(eastl::string_view view)
  {
    const auto result = Container::addNameId(view.data(), view.size());
    NAU_FAST_ASSERT(result < eastl::numeric_limits<eastl::underlying_type_t<EnumType>>::max());
    return static_cast<EnumType>(result);
  }

  EnumType getNameId(eastl::string_view view) const
  {
    auto result = Container::getNameId(view.data(), view.size());
    return result < 0 ? EnumType::Invalid : static_cast<EnumType>(result);
  }

  eastl::string_view getName(EnumType id) const { return getNameCstr(id); }

  const char *getNameCstr(EnumType id) const
  {
    auto result = Container::getName(static_cast<int>(eastl::to_underlying(id)));
    return result == nullptr ? "INVALID_NAME_ID" : result;
  }
};
