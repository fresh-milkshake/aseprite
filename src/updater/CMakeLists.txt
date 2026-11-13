# ASEPRITE
# Copyright (C) 2020-2024  Igara Studio S.A.
# Copyright (C) 2001-2017  David Capello

# OS information for the Lua app.os object.
add_library(updater-lib user_agent.cpp)
target_link_libraries(updater-lib laf-base ver-lib)
