# Trigger MIDI Notes

Convert modular triggers and gates into MIDI notes on the **Expert Sleepers
disting NT**. Use up to eight independently configured trigger lanes to play a
drum machine, sampler, software instrument, or MIDI-enabled algorithm inside
the NT.

Each lane listens to one NT bus and sends a chosen note to a chosen MIDI
destination and channel. Note length follows the input gate, and note-on
velocity is fixed at **127**. The plug-in reads its input buses without changing
their audio or CV signals.

## Installation

1. Download **`trigger_midi_notes-plugin.zip`** from the
   [latest release](https://github.com/thorinside/trigger-midi-notes/releases/latest).
2. Extract the archive and merge its `programs` folder into the root of the
   NT's MicroSD card. The installed file should be:

   ```text
   /programs/plug-ins/trigger_midi_notes.o
   ```

3. Reinsert the card and restart the NT so it scans the plug-in.
4. Find **Trigger MIDI Notes** in the **Add algorithm** menu. Select **Load**
   first if shown, then **Add**. It is tagged **Utility**.
5. Choose the **Channels** specification when adding the algorithm: **1–8**
   trigger lanes, default **1**.

Release **v0.2.0** requires **disting NT firmware 1.15.0 or newer**: it uses
plug-in API v13, introduced in firmware 1.15.0. See the official
[firmware updates and manuals](https://www.expert-sleepers.co.uk/distingNTfirmwareupdates.html).

## First patch

1. Patch a trigger or gate source into NT input **1**.
2. Add the plug-in with **Channels = 1**.
3. On its **Ch 1** parameter page, leave **Input = 1**, **MIDI Ch = 1** and
   **Note = 60**.
4. Set **MIDI Dest** to **USB** for a computer, or **Breakout** for a connected
   MIDI output breakout and external instrument.
5. Set the receiving instrument or DAW track to listen to that MIDI port and
   channel **1**.
6. Send a gate. The plug-in sends note 60 when the gate rises and releases it
   when the gate falls.

`Channels` counts trigger lanes; **MIDI Ch** selects the MIDI channel used by
each lane. Multiple lanes can send different notes on the same MIDI channel.

## Controls

Each lane has its own page, **Ch 1** through **Ch 8**, containing four controls.
With more than one lane, parameter names also receive a lane-number prefix.

| Control | Range / choices | Default | Function |
| --- | --- | --- | --- |
| **Input** | `None` (0), buses **1–28** | Lane number: Ch 1 → bus 1, Ch 2 → bus 2, etc. | Bus containing this lane's trigger or gate. `None` disables input processing for that lane. |
| **MIDI Ch** | **1–16** | **1** | MIDI channel for outgoing note messages. |
| **MIDI Dest** | **Breakout**, **USB**, **Select Bus**, **Internal** | **Breakout** | Destination for this lane's messages. |
| **Note** | MIDI note number **0–127** | **60** | Note sent on each gate. Use the note number to match the receiver; octave-name conventions vary between instruments. |

Every lane initially uses MIDI channel **1**, destination **Breakout** and note
**60**. Set each lane's note and routing for your patch.

The current plug-in exposes buses **1–28** even on firmware with more buses.
These are the 12 physical input buses, eight output buses, and first eight aux
buses. To trigger it from another NT algorithm, route that algorithm's gate to
an available bus in this range and select that bus as **Input**.

### MIDI destinations

| Destination | Where the notes go |
| --- | --- |
| **Breakout** | The NT's MIDI output header, used with a suitable MIDI breakout connection. |
| **USB** | The NT's USB MIDI connection to a computer or other USB host. |
| **Select Bus** | The NT's Select Bus connection; requires a compatible Select Bus setup. |
| **Internal** | MIDI-enabled algorithms within the NT. Set the receiving algorithm to listen on the matching MIDI channel. |

Each lane selects one destination. Different lanes can use different
destinations and MIDI channels.

## Gate and note behavior

- A low lane switches on when its input reaches **1.0 V or higher**, sending
  **Note On** with velocity **127**.
- An active lane switches off when its input reaches **0.1 V or lower**, sending
  **Note Off** with release velocity **0**.
- Between those thresholds, the previous gate state is retained. This
  hysteresis helps prevent repeated notes from small voltage fluctuations.
- A held gate sends one note-on, followed by one note-off when it falls. It
  must return to **0.1 V or lower** before another rise can trigger a new note.
- Short trigger pulses produce short notes. Use a longer gate when the
  receiving instrument needs a sustained note; there is no fixed-duration or
  pulse-stretch setting.

The **Note** parameter chooses pitch. There is no separate pitch-CV input or
velocity input, and gate amplitude does not change note-on velocity.

### Changing settings while playing

Let every active gate return low **before changing Input, MIDI Ch, MIDI Dest or
Note**, disabling/bypassing the algorithm, or removing it from the preset.
The plug-in uses the current settings for each outgoing message; it does not
remember the original note and destination for a later note-off. Changing
settings during a held gate can therefore leave the original note playing.
There is no built-in panic control. If a note sticks, use the receiving
instrument's panic/all-notes-off function.

If two lanes overlap on the same destination, MIDI channel and note number,
one lane's note-off may stop the other's note on the receiver. Use different
notes or MIDI channels when those lanes need independent note lengths.

## Example: four modular drum triggers

Add the plug-in with **Channels = 4**. Connect four trigger sources to inputs
**1–4**, keep each lane's default **Input**, and configure:

| Page | MIDI Ch | MIDI Dest | Note |
| --- | --- | --- | --- |
| Ch 1 | 10 | USB | 36 |
| Ch 2 | 10 | USB | 38 |
| Ch 3 | 10 | USB | 42 |
| Ch 4 | 10 | USB | 46 |

Route the NT's USB MIDI output to a drum instrument listening on channel 10.
Choose note numbers that match that instrument's drum map. For an external
drum machine connected through the MIDI breakout, change each lane's
**MIDI Dest** to **Breakout**.

## Troubleshooting

- **Plug-in is missing:** check the `.o` file's location, firmware compatibility
  and the algorithm-list tag filter, then restart the NT to rescan. The
  **Misc / Plug-ins / View info…** menu shows whether the file passed its scan.
- **No notes reach the receiver:** check **Input**, **MIDI Dest**, **MIDI Ch**,
  the physical/USB connection and the receiver's MIDI routing. The default
  destination is **Breakout**, so select **USB** explicitly for a computer.
- **Only the first trigger works:** check that the input returns to **0.1 V or
  lower** between pulses.
- **Notes are very short:** note duration follows the gate; lengthen the pulse
  at its source or use a receiving instrument's one-shot mode.
- **A note stays on:** bring the gate low with its original settings intact,
  or use the receiver's panic function. See the settings-change guidance above.

The implementation is in [trigger_midi_notes.cpp](trigger_midi_notes.cpp).
