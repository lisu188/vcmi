# scripts/examples/onTurn.py
# Python twin of onTurn.lua: a general engine-event script. Declare it in a mod's
# "scripts" content (see config/schemas/script.json) with
#   { "source" : "examples/onTurn.py", "implements" : "ON_PLAYER_TURN_START" }
# It is fired server-side from NewTurnProcessor via scripting::Context::callGlobal.
# Requires an engine built with ENABLE_PYTHON.
#
# Engine hooks are module-level functions receiving one dict of identifiers and
# returning plain data (or None). This script runs sandboxed: only `math` and the
# embedded `vcmi` module are importable, and print() goes to the VCMI log.

def onPlayerTurnStart(params):
    print("[onTurn.py] player", params["player"], "starting day", params["day"])
