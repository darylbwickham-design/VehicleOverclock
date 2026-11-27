# Vehicle Overclock

**Vehicle Overclock is an unstable public testing alpha. Expect bugs. No support is available at this stage.**

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
| 3 | 250% | 1.75× Explorer, 1.90× other vehicles | 4.00× |

Factory Carts stay fuel-free. Locomotives draw additional rail power. Actual speed depends on terrain, load and traction.

## Vehicle Engineering research

Research **Vehicle overclocking** once at the MAM for 20 Iron Plates and 30 Wire. Everyone in the session can then fit Power Shards through supported vehicle menus. Saves that already completed Chassis calibration retain this unlock.

| Branch | Research |
| --- | --- |
| Fleet efficiency | Four ranks branch directly from Vehicle overclocking. At 250% clock, each rank lowers fuel or rail power demand by 0.5× stock. The progression is 4×, 3.5×, 3×, 2.5× and 2×. At intermediate clocks each rank removes one sixth of the original surcharge. At 100% clock the rate stays at 1×. Booster fuel is metered separately. |
| Tractor | Three ranks each of tyre grip, downforce and brakes lead to electronic stability control. The Tractor chassis also unlocks **Tractor · Forward rocket** for 12 Steel Pipes and 4 Rotors (30 seconds). Hold Boost while grounded for forward thrust, including from a standstill. |
| Explorer | All-terrain tyres, low-mounted ballast, roll dampers, rally brakes and a rally stabiliser. The three ballast levels add 6%, 10% and 14% chassis mass while overclocked. A separate three-rank steering rack adds 10%, 20% and 30% steering lock while overclocked. |
| Truck | Haul tyres, brake servo and load stabiliser for both freight chassis. |
| Controlled drift | For manually driven Explorers and Trucks. Steer above 29 km/h with the tyres on the ground, then hold **Left Ctrl** to swing the rear outward while the booster helps maintain corner-entry speed. Braking or using the handbrake cuts the assist. In **Mods**, select **Vehicle Overclock**, click **Boost hotkey** and press your preferred key. The same key is listed under **Vehicles → Vehicle drift** in the game controls. |

The MAM uses aligned research tiers with clear rows between them. Tractor handling is on the left, Explorer handling and racing controls in the centre, with Truck handling and fleet efficiency on the right. Existing research retains its unlocks.

The Explorer has nine additional racing studies.

| Research | Effect |
| --- | --- |
| Drift control I and II | Rear slip builds over 0.6 then 0.4 seconds. Steer into the drift to tighten the curve, or countersteer to widen and straighten it. Holding the opposite direction keeps the original drift side until Boost is released. |
| Corner exit | Rear grip returns over 0.45 seconds. The chassis settles toward its travel direction after Boost is released. |
| Air control I, II and III | Improve airborne rotation and damping. A/D controls yaw. Hold **Air control**, default **Left Alt**, for A/D roll and W/S pitch. Vehicle steering, throttle and brake bindings are used, including user remaps. Orientation uses no booster fuel. |
| Air thrusters | Hold Boost and a direction to adjust horizontal flight. Without a direction input, thrust is forward. Uses the sulphur-based booster reserve. |
| Landing recovery I and II | Cushion the approach near a surface and damp rebound, pitch and roll at touchdown. Rank II also prepares a drift when Boost and steering are held before landing. |

Both **Boost hotkey** and **Air control hotkey** can be changed in the mod options or the game's Vehicle controls. Full Explorer steering research also provides speed-sensitive yaw and cornering assistance during ordinary overclocked driving.

The Tractor has its own three-rank **Air control** branch with the same modifier and directional controls. Full air research on either chassis permits continuous pitch and roll for complete flips, with damping on release. Rotation requires no booster fuel. The Tractor's forward rocket research provides grounded thrust.

Shift-click Compacted Coal, Packaged Turbofuel, Packaged Rocket Fuel or Packaged Ionized Fuel from your inventory while the **Overclocking** tab is selected, or drag a stack into the booster fuel slot below the shards. Click the slot to return unopened fuel. Dismantling also returns unopened booster fuel through the normal dismantle refund, with the standard overflow crate if your inventory is full. The partly burned unit has already been consumed. Each vehicle keeps its own booster fuel and partially used charge between drives. The HUD beside the speedometer shows unopened units and the current charge. Ground and air boosters burn 100 MJ per second of firing. Releasing Boost cuts thrust.

Speed-sensitive grip, braking and stability build up between 55% and 115% of stock geared speed. The Explorer's third shard adds less speed than the first two.

## Installation

Install **Vehicle Overclocking** with Satisfactory Mod Manager. Its mod reference is `vehiclephysics`. If you installed the earlier private ZIP by hand, close the game and move the old `FactoryGame/Mods/VehicleOverclock` folder outside `Mods` before installing the SMM version. Both builds contain the same native module.

To build from source, put this repository in `Mods/vehiclephysics` in the matching SML starter project. Build the **FactoryGameSteam** target for **Win64 Shipping** using the Coffee Stain Unreal Engine. The native module name is `VehicleOverclock` to preserve existing save references.
