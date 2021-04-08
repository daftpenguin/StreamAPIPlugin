**Warning: Consider the current version of this plugin as a limited, beta release. This plugin relies on many newly added features to BakkesMod and thus there may be some bugs. This version only exposes most car loadout items, camera settings, and sensitivies. Car primary and accent colors will be added in a future update, as well as bindings and video settings.**

StreamAPI plugin exposes data about your game that can be queried by local bots and/or other software for viewers to view live infor about your game. Loadout data includes support for BakkesMod item mod items and Rainbow Car and AlphaConsole plugin settings.

This plugin works by launching a lightweight http server running on its own thread that can be queried for information, with very little to no impact on game performance. Data responses are only generated when the data changes, not per request. The port for this server can be configured in the plugin's settings of the F2 menu (default is 10111), or in the F6 console by typing `streamapi_port port`, replacing `port` with any number between 1024 and 49151, and pressing enter. The server will restart whenever the port is changed.

The plugin can be used with a local stream bot such as Streamlabs Chatbot to allow viewers to request information about your game. This will not work with bots hosted elsewhere, such as Nightbot.

The API exposes 6 commands: `loadout`, `sens`, `camera`, `video`, `bindings` and `training`. This data can be queried from the local URL: `http://127.0.0.1:10111/cmd?cmd=cmd_name&args=optional_arg`. With the plugin running, you should be able to open `http://127.0.0.1:10111/cmd?cmd=loadout` in a browser.

# Streamlabs Chatbot Setup

Open `Commands` section in Chatbot and add a command and give it a name. Then set the command response to `$readapi(http://127.0.0.1:10111/cmd?cmd=cmd_name&args=optional_arg)` but change `cmd_name` and `optional_arg` according to the table below. For commands with no args, you can completely remove `&args=optional_arg`. **If you changed streamapi_port, replace 10111 with the port number you chose.**

| cmd | args |
| - | - |
| loadout | Optional args: body, car, skin, decal, wheels, boost, antenna, topper, engine, trail, goal explosion, ge. No arg = shows full loadout + bakkesmod item code. Setting args to `$dummyormsg` will allow viewers to add their own arg, or none at all. |
| sens | none |
| camera | none |
| video | none |
| bindings | Optional args: kbm. No arg = controller |
| training | none |

### Example commands:

| Command | Response |
| - | - |
| !loadout | `$readapi(http://127.0.0.1:10111/cmd?cmd=loadout&args=$dummyormsg)` |
| !wheels | `$readapi(http://127.0.0.1:10111/cmd?cmd=loadout&args=wheels)` |
| !bindings | `$readapi(http://127.0.0.1:10111/cmd?cmd=bindings&args=kbm)` |
| !training | `$readapi(http://127.0.0.1:10111/cmd?cmd=training)` |

# Bugs / Issues

If you run into any issues with setting this up, or crashes to your game, PLEASE contact me as I would like to fix them.

# Contact

* [@PenguinDaft](https://twitter.com/PenguinDaft)
* Discord: DaftPenguin#5103
* Twitch: DaftPenguin
* Reddit: /u/DaftPenguinRL