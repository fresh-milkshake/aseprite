# An existing light-theme choice survives, but branching Undo is migrated.
fork_settings="$t/fork-existing-settings"
mkdir -p "$fork_settings"
cat > "$fork_settings/aseprite.ini" <<'INI'
[general]
recent_items = 0
[theme]
selected = default
[undo]
allow_nonlinear_history = true
INI
ASEPRITE_USER_FOLDER="$fork_settings" "$ASEPRITE" --batch \
    --script-param expected-theme=default --script scripts/fork_defaults.lua \
    || fail "Existing theme preference or linear-undo migration failed"
