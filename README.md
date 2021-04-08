**Warning: Consider the current version of this plugin as a limited, beta release. This plugin relies on many newly added features to BakkesMod and thus there may be some bugs. This version only exposes most car loadout items, camera settings, and sensitivies. Car primary and accent colors will be added in a future update, as well as bindings and video settings (potentially needing updates to BakkesMod).**

StreamAPI plugin exposes data about your game that can be queried by local bots and/or other software for viewers to view live information about your car loadout, kbm/controller bindings, sensitivities, and video settings.

Car loadout data will only show the items that are equipped to the car currently showing on the screen. Loadout data also supports items equipped via BakkesMod's item mod (and generates a BakkesMod code for all items shown between in-game items and BakkesMod item), as well as Rainbow Car plugin's settings and Alpha Console items, if they are enabled.

This plugin works by launching a lightweight http server running on its own thread that can be queried for information, with very little to no impact on game performance. Data responses are only generated when the data changes, not per request. The port for this server can be configured in the plugin's settings of the F2 menu (default is 10111), or in the F6 console by typing `streamapi_port <port>`, replacing `<port>` with any number between 1024 and 49151, and pressing enter. The server will restart whenever the port is changed.

The plugin can be used with a local stream bot such as Streamlabs Chatbot to allow viewers to request information about your game. This will not work with bots hosted elsewhere, such as Nightbot.

The API exposes 5 commands: `loadout`, `sens`, `camera`, `video`, and `bindings`. This data can be queried from the local URL: `http://127.0.0.1:10111/cmd?cmd=<cmd>&args=<optional parameters>`. With the plugin running, you should be able to open `http://127.0.0.1:10111/cmd?cmd=loadout` in a browser and view your loadout data.

# Streamlabs Chatbot Setup

The `loadout` command exposes data about the items you have equipped to your car. This command allows the following additional arguments to be passed in to retrieve info about individual items: `body` or `car`, `skin` or `decal`, `wheels`, `boost`, `antenna`, `topper`, `engine`, `trail`, `goal explosion` or `ge`. If no argument is given, all items are given, along with a BakkesMod item mod code that can be used for equipping ALL items besides Rainbow Car and AlphaConsole plugin settings. You control how you would like to expose this data to your viewers via your stream bot. For instance, you can allow viewers to specify the item arguments themselves, or you can choose to create invidiaul commands for each item.

To make a general `!loadout` command which allows users to pass in an optional parameter:

1. Open the `Commands` section in Streamlabs Chatbot.
2. Add a command, name it `!loadout` (or any name you would like to give it).
3. Set the command's response to: `$readapi(http://127.0.0.1:10111/cmd?cmd=loadout&args=$dummyormsg)`.
    - Note: If you modified the `streamapi_port`, you will need to replace `10111` with whatever port number you chose.

The `$dummyormsg` parameter will allow the viewers to pass any of the previously mentioned optional arguments, or nothing at all.

To instead make a command for say `!wheels`, you can just add the command named `!wheels` and simply replace `$dummyormsg` with `wheels`: `$readapi(http://127.0.0.1:10111/cmd?cmd=loadout&args=wheels)`.

Do NOT change `cmd=loadout`. That value is NOT related to the name you are assigning the command in Streamlabs and is the name of the command inside the plugin.

For `sens`, `camera`, and `video` commands, simply create commands for them with the response set to `$readapi(http://127.0.0.1:10111/cmd?cmd=sens)` (replace `sens` with `camera` or `video` for the other 2 commands).

The `bindings` command by default assumes you play on controller. If you are a kbm player, create your `bindings` command's response to `$readapi(http://127.0.0.1:10111/cmd?cmd=bindings&args=kbm)`. If you are not a kbm player, simply remove `&args=kbm`.

# Bugs / Issues

If you run into any issues with setting this up, or crashes to your game, PLEASE contact me as I would like to fix them.

# Contact

* [@PenguinDaft](https://twitter.com/PenguinDaft)
* Discord: DaftPenguin#5103
* Twitch: DaftPenguin
* Reddit: /u/DaftPenguinRL