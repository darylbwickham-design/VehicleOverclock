# Vehicle Overclock

**Private testing only. Vehicle Overclock is an unstable alpha. Expect bugs. No support is available at this stage.**

Fit Power Shards to boost vehicle speed at a higher fuel or rail power cost, and research high-speed handling upgrades at the MAM.

For the Windows Steam client, game build 520140 and SML 3.12.1.

You're welcome to use, modify, and share the mod's source, including in your own mods. Keep the copyright and license notice with copies or substantial portions of the code. [MIT License](LICENSE)

## Vehicle controls

Open the vehicle's own interaction window and select **Overclocking**, the third tab. On a vehicle with one stock page, **Vehicle performance** is the second tab. The overclocking controls use the same window, tab buttons, inventory and close button as the stock vehicle UI. Vehicles without a native interaction window cannot fit shards.

The tab uses the machine's overclock panel. Click an empty slot to fit shards up to that slot, or drag a Power Shard from your inventory onto an empty slot to fit one. Click or right-click a filled slot to return one shard.

Click or drag the segmented bar, or click the clock percentage, type a value, and press Enter. You can also enter a rated speed multiplier. Raising the clock fits the required shards from your inventory. Lowering it leaves the fitted shards available. Vehicle clocks range from 100% to 250%.

You can fit shards while parked and then enter the driver's seat. Leaving the seat returns your shards and restores the stock tuning. Surplus shards drop beside the vehicle if your inventory is full.

| Shards | Clock | Rated speed | Fuel burn or rail power |
| ---: | ---: | ---: | ---: |
| 0 | 100% | 1.00× | 1.00× |
| 1 | 150% | 1.30× | 1.85× |
| 2 | 200% | 1.60× | 2.85× |
| 3 | 250% | 1.90× | 4.00× |

Factory Carts stay fuel-free. Locomotives draw additional rail power. Actual speed depends on terrain, load and traction.

## Vehicle Handling research

The **Vehicle Handling** tree in the MAM applies to the whole session. **Chassis calibration** opens three branches. Each rank requires the preceding rank in that branch. Completing all three branches unlocks **Electronic stability control**.

| Research | Materials | Time | Effect at high speed |
| --- | --- | ---: | --- |
| Chassis calibration | 20 Iron Plates, 30 Wire | 15 s | Damps excessive yaw while the tyres are on the ground |
| Tyre grip I | 40 Screws, 8 Iron Plates | 20 s | Up to 12% additional tyre grip |
| Tyre grip II | 4 Rotors, 12 Steel Pipes | 35 s | Up to 24% additional tyre grip |
| Tyre grip III | 30 Rubber, 3 Motors | 60 s | Up to 36% additional tyre grip |
| Downforce I | 12 Copper Sheets, 16 Iron Rods | 20 s | Up to 8% additional tyre loading relative to vehicle weight |
| Downforce II | 6 Reinforced Iron Plates, 12 Steel Pipes | 35 s | Up to 16% additional loading |
| Downforce III | 6 Modular Frames, 24 Rubber | 60 s | Up to 24% additional loading |
| Braking I | 12 Iron Plates, 10 Cable | 20 s | Up to 15% more service braking and 0.18 m/s² extra road deceleration off throttle |
| Braking II | 4 Stators, 12 Steel Pipes | 35 s | Up to 30% and 0.36 m/s² |
| Braking III | 3 Motors, 24 Rubber | 60 s | Up to 45% and 0.54 m/s² |
| Electronic stability control | 8 Circuit Boards, 4 Motors | 90 s | Corrects sideways slip, excessive yaw and roll in fast road turns |

Bonuses apply during manual driving above 100% clock. They start at 55% of each vehicle's stock geared top speed and reach full strength at 115%. Chassis forces require tyre contact. Locomotives inherit the braking upgrades on the driven locomotive's air brakes.

## Installation

Install **Vehicle Overclocking** from its Hidden listing with Satisfactory Mod Manager. Its mod reference is `vehiclephysics`. If you installed the earlier private ZIP by hand, close the game and move the old `FactoryGame/Mods/VehicleOverclock` folder outside `Mods` before installing the SMM version. Both builds contain the same native module.

To build from source, put this repository in `Mods/vehiclephysics` in the matching SML starter project. Build the **FactoryGameSteam** target for **Win64 Shipping** using the Coffee Stain Unreal Engine. The native module name is `VehicleOverclock` to preserve existing save references.
