Thought for a few seconds

Yes — the separate simulation tick is the right mental model.

And your proposed handshake is too chatty for normal play.

The host does not need to wait for a per-command:

execute
confirm
confirm-the-confirmation
okay-now-you-may-continue

If you do that, you will recreate the exact “everything feels sticky” problem you dislike.

The cleaner model

For a real-time-pausable strategy game, separate these three things:

simulation ticks
wall-clock / render time
calendar time

Those are not the same thing.

A good structure is:

the simulation runs in discrete ticks: sim_tick = 1001, 1002, 1003...
the host decides when each sim tick is allowed to execute
the in-game date advances according to rules tied to sim ticks
pause just means the host stops advancing the authorized sim tick
speed 1/2/3/4 just means the host changes how fast sim ticks are released

So yes: have a tick system separate from dating.

That is probably the cleanest way to think about it.

The important correction

In lockstep, the host usually should not do:

client sends command
host executes command immediately
host tells client what tick it executed on
client executes it
client confirms execution
host says okay advance

That is more like an authoritative-state server with extra handshakes.

Instead, the host should do something more like:

client sends command
host assigns it to future sim tick T
host broadcasts “these are the commands for tick T”
every peer stores that command in its timeline
when local sim reaches tick T, every peer executes it locally
peers periodically report checksums / acknowledgments of tick progress

So the host is primarily authorizing the timeline, not micromanaging every execution.

The key idea: schedule into the future

This is the part that makes real-time pausable lockstep workable.

Suppose the current confirmed sim tick is 5000.

A client issues an order now.

The host does not try to make it happen on 5000 or 5001.
It might assign it to 5008.

That gives enough network cushion so everyone receives the same command before tick 5008.

So the real flow is:

Client issues command locally
-> client predicts visuals immediately
-> client sends command to host
-> host validates and assigns target sim tick = current_host_tick + input_delay
-> host broadcasts confirmed command with target tick
-> all peers queue it for that tick
-> when tick arrives, all peers execute it locally

That is the normal lockstep shape.

No extra “did you execute this exact command?” round trip is needed for every action.

What acknowledgments are actually for

You still want acknowledgments, just not at the per-command/per-execution level.

Use them for things like:

“I have received all commands through tick X”
“I have simulated through tick X”
“My checksum at tick X is 0xABCDEF”
“I am missing command packet for tick X”
“I am behind”

That is much lighter and more scalable.

So instead of:

did you execute command 712 yet?

you ask:

are you caught up through tick 5010, and does your checksum match?

That is the better abstraction.

Real-time pausable changes pace, not logic

Paradox-style time can be misleading because it looks continuous, but under the hood what you want is still:

a discrete simulation timeline
host-controlled release of ticks
commands scheduled against ticks

The only difference from Factorio is that tick release is not always constant.

So you might think of it like this:

Simulation layer
tick 1000
tick 1001
tick 1002
Time control layer
paused: release no new ticks
speed 1: release 5 ticks/sec
speed 2: release 10 ticks/sec
speed 3: release 20 ticks/sec
Calendar layer
every N sim ticks, advance one day
or one month
or whatever your pacing rule is

So yes, your idea is good:
the host controls at what ticks time advances.

That is much cleaner than tying network scheduling directly to “March 4th, 1452.”

Two common ways to handle it
1. Strict lockstep barrier

The host only allows tick T once all players have provided required inputs / caught up enough.

This is safest, but can feel sluggish if one player is slow.

2. Buffered lockstep

The host keeps a rolling command buffer several ticks ahead.

Example:

host is currently simulating tick 1000
commands are being collected and finalized for ticks 1006+
clients can be slightly behind or ahead in buffered state
everyone stays inside a safe window

This is usually what you want.

That way minor latency does not immediately stall the whole game.

Your “host says okay to progress” instinct

That instinct is not wrong — it just belongs at the tick frontier, not after every command.

In other words, the host can absolutely control progression like this:

clients may not simulate past authorized tick T_max
host periodically raises T_max
clients simulate forward up to that limit if they have the needed commands

That is a very useful mental model.

So instead of:

“okay, now execute that one build command”

it becomes:

“the frontier is now tick 8120; you may simulate up to there”

That is much better.

A practical communication model

Think in terms of these message types:

From client to host
command submission
pause/unpause request
checksum report
progress report: “I’ve simmed through tick X”
missing-data request
From host to clients
confirmed command with assigned tick
authorized tick frontier
pause/speed state
checksum mismatch / resync request
full snapshot for rejoin/resync

That is enough for the core system.

What the host actually authorizes

The host should authoritatively define:

current canonical sim tick
future authorized frontier
command-to-tick assignments
pause state
game speed
resync / snapshot state
join/rejoin state

The host does not need to individually confirm every local sim step unless something has gone wrong.

How this avoids desyncs

Your concern was basically:

what stops a client from advancing without having committed all commands?

The answer is:

the client is not allowed to advance past the latest authorized tick for which it has the finalized command set.

So a client should only execute tick T when:

it knows tick T is authorized
it has the commands for tick T
it has finished tick T-1

If any of those fail, it waits.

That is the barrier.

You do not need the client to say “I executed command A, B, and C” individually.
The tick itself is the unit of synchronization.

The best unit is the tick, not the command

That is the big conceptual shift.

In this style of game:

commands are scheduled inputs
ticks are synchronization units
checksums are correctness checks

So the protocol should revolve around tick numbers.

Separate simulation tick from date absolutely

This part is especially important for your game.

You might end up with something like:

sim_tick: the actual deterministic update counter
date: game-world displayed time
speed_setting: how fast host advances the tick frontier
subphase: optional, if one date step contains multiple sim steps

For example:

20 sim ticks = 1 in-game day
600 sim ticks = 1 in-game month

Or:

sim ticks are purely internal and the calendar updates only when enough work has accumulated

Either way, the network layer should care about sim_tick, not date.

Pause and speed become easy

Once you do this:

Pause

Host stops increasing authorized frontier.

Unpause

Host resumes increasing it.

Change speed

Host changes how quickly the frontier increases.

That is much easier than trying to make “time flow” itself the synchronized object.

Where prediction fits

Prediction should still be local and visual:

build ghost appears immediately
order line appears immediately
pending treasury/UI value updates immediately

But the real command is only part of the simulation once the host assigns it to tick T and broadcasts that.

If the host rejects or modifies it, the predicted overlay reconciles.

That gives you responsiveness without messing up the sim timeline.

One subtle point: not all commands need the same delay

You may eventually want different categories:

normal gameplay commands: scheduled at current_tick + input_delay
pause/speed commands: special handling, often host-only or host-priority
UI-only local actions: immediate, not networked
diplomacy or modal actions: maybe scheduled but visually non-predicted

But early on, a fixed input delay window is the simplest clean model.

So, bluntly

Your current model has the right instinct but too many handshakes.

The simpler and better version is:

Client issues command
-> local UI predicts
-> sends command to host
-> host validates and assigns future sim tick T
-> host broadcasts command for tick T
-> all peers queue it
-> all peers execute it when sim tick T arrives
-> peers periodically report progress/checksums
-> host advances authorized tick frontier as appropriate

And yes:

have a simulation tick system completely separate from the calendar/date system.
That is probably the right foundation for a Paradox-like real-time-pausable game with lockstep networking.

The short answer to your main question

You do not need multiple back-and-forths per command.

You mostly need:

command submission
host tick assignment + broadcast
periodic progress/checksum acknowledgments
host-controlled tick frontier

That is the cleaner version of what you were aiming at.

If you want, the next useful step is to sketch the exact timeline of:
“player clicks build road at local time X, with 120 ms ping, while the game is running at speed 3”
so you can see how the buffer/frontier system actually feels in practice.

Yes. For a listen-server setup, the host player should normally have one authoritative simulation world, not a separate “server world” and “client world.”

The host process contains two logical roles:

Host authority: validates commands, assigns execution ticks, controls time, compares checksums.
Local player client: gathers input, displays UI, and submits commands.

But both roles ultimately reference the same authoritative WorldState.

Host process
├── LocalClient
│   ├── Input
│   ├── UI
│   └── Prediction overlays
├── HostAuthority
│   ├── Command validation
│   ├── Tick scheduling
│   └── Network coordination
└── Authoritative WorldState
    └── Deterministic simulation

The local client should not maintain a second complete simulation copy merely to imitate a remote client.

What the host still needs separately

The host will probably have some client-side predicted presentation state, but this should be lightweight:

pending construction ghosts
order arrows
projected treasury values
commands waiting for their execution tick
temporary UI states

That is not a second world simulation. It is more like:

Rendered host view =
    authoritative WorldState
    + local pending-command overlays

Once the command executes, the overlay disappears because the authoritative world now contains the real result.

Command flow for the host player

The host should still use the same command path:

Host player clicks
→ LocalClient creates command
→ local transport sends it to HostAuthority
→ HostAuthority validates and schedules it
→ command enters the canonical timeline
→ authoritative WorldState executes it

The important part is that the host player does not bypass validation or directly mutate the world.

You can use an in-memory transport call rather than serializing and sending a packet through the networking stack, but logically it should look like a client submission.

Remote clients

Each remote machine does need its own deterministic world-state copy:

Host machine:       authoritative WorldState
Remote client A:    replicated deterministic WorldState
Remote client B:    replicated deterministic WorldState

They all execute the same command timeline. “Authoritative” here means the host decides which commands enter the timeline and when; it does not necessarily mean the clients receive every resulting state change from the host.

Why not run two worlds on the host?

Running both a server simulation and a local-client simulation would:

roughly double simulation memory and CPU usage for the host
create opportunities for the two local copies to diverge
complicate presentation binding
require local snapshot synchronization for no gameplay benefit

It can still be useful as a debug mode. Running two independent simulations in one process and comparing their hashes is an excellent determinism test. But it should not be the normal host gameplay architecture.

Singleplayer

Singleplayer should likewise have one simulation world:

LocalClient
→ LocalHostAuthority
→ one authoritative WorldState

With:

in-memory transport
one-tick command delay
no active network peer
the same validation, scheduling, execution, and prediction systems
Dedicated servers

A dedicated server would simply omit the local player-facing pieces:

Dedicated server
├── HostAuthority
├── Network transport
└── Authoritative WorldState

The design therefore supports all three modes without changing the simulation:

Mode	Simulation worlds in process	Local player
Singleplayer	1	Yes
Player-hosted multiplayer	1	Yes
Dedicated server	1	No

So the clean rule is:

One deterministic world per participating process, including the host. The host’s copy is authoritative; its local client interacts with that copy through the same scheduled-command architecture as everyone else.

Starting in GDScript is sensible. You should use it to prove the architecture before committing to C++.

The clean split is:

Keep in GDScript

Use GDScript for the parts that are closely tied to Godot, iteration speed, and presentation:

UI
input handling
camera and map interaction
menus
selection
tooltips
command construction
predicted visuals
construction ghosts
order arrows
debug panels
networking glue
lobby flow
save/load orchestration
scene-tree interaction
temporary prototyping code

GDScript is especially good for answering:

does the command pipeline feel right?
does pause behave correctly?
does prediction hide the delay?
is the host/client separation understandable?
are the tick rules workable?

That is what you are doing now, and it is the right use of GDScript.

Put in C++ eventually

Move the authoritative simulation core into C++ once the design is stable.

That likely includes:

WorldState
location arrays
country arrays
population arrays
market data
deterministic tick execution
calendar advancement
command application
deterministic RNG
checksum generation
simulation phase ordering
multithreaded jobs
pathfinding or route calculation
large-scale AI calculations
serialization of authoritative state

The C++ side should ideally know very little about the scene tree.

It should receive something like:

commands for tick T

and return or expose:

updated deterministic state
events produced this tick
presentation data that changed
checksum
The boundary

Think of GDScript as the shell and C++ as the machine.

GDScript
├── UI
├── input
├── prediction
├── networking interface
├── map rendering
└── sends commands
        ↓
C++ simulation
├── authoritative world
├── tick timeline
├── calendar
├── systems
├── RNG
├── jobs
└── state hashes
        ↓
GDScript reads results

The important boundary is not language. It is authority.

GDScript can create a command:

BuildRoad(location_a, location_b)

but it should not directly alter authoritative road data.

C++ receives the command on its scheduled tick, validates it against the simulation state, and commits the result.

What to prototype first in GDScript

Before moving anything, prove these systems in GDScript:

Constant synchronization tick
Separate calendar progression
Pause and speed commands
Local host/client distinction
Scheduled command execution
One-tick singleplayer delay
Predicted presentation
Command recording
Replay
Deterministic state hashing

Use a tiny fake world.

For example:

ten locations
two countries
one resource
one build command
one tax-setting command
pause and speed controls

You do not need the real economy yet.

Once this skeleton works, you will understand the interfaces your C++ module actually needs.

What not to move too early

Do not move something to C++ merely because it “sounds low-level.”

Keep it in GDScript until one of these becomes true:

profiling shows it is expensive
it contains large authoritative arrays
it must be multithreaded
it is central to deterministic state
GDScript object overhead becomes substantial
frequent language-boundary calls are becoming a problem

Prematurely moving half-designed systems to C++ will slow iteration and lock in bad interfaces.

What should probably stay out of C++

Unless profiling strongly argues otherwise, do not put these in C++:

menus
buttons
tooltips
normal UI state
input mapping
visual prediction
map-mode presentation
animation
camera logic
most debug tooling
lobby UI

They change frequently and gain little from C++.

Be careful with per-entity crossings

The worst architecture would be:

GDScript loops over 100,000 pops
→ calls C++ once per pop
→ reads one value

The language boundary should be coarse.

Prefer:

GDScript asks for one province summary

or:

C++ fills a packed array of visible location values

or:

C++ returns all changed location IDs for this tick

You want a few large calls, not thousands of tiny calls.

A useful data split

You will probably end up with three forms of state.

Authoritative state — C++

Everything required to reproduce the simulation:

populations
resources
ownership
treasury
command timeline
RNG state
date
calendar accumulator
Presentation cache — GDScript or renderer-facing C++

Only what the UI/map needs:

current selected-location data
map color values
visible icons
changed locations
summarized country statistics
Predicted local state — GDScript

Pending visual intent:

ghost buildings
unconfirmed orders
projected values
pending command markers

That division is more important than the exact language choice.

Networking split

I would initially keep networking coordination in GDScript:

connecting peers
RPC or packet reception
lobby state
forwarding messages
host/client role setup

But keep packet contents and simulation semantics independent of GDScript objects.

Your command format should be simple and serializable:

command_type
player_id
sequence
execution_tick
integer payload

Later, if networking throughput or serialization becomes a bottleneck, you can move packet encoding into C++. The higher-level session management can still remain in GDScript.

Multithreading split

Multithreaded simulation work belongs in C++.

GDScript can start or request simulation work, but the internal worker system should remain inside the C++ simulation module.

A good eventual interface is:

simulation.step_tick()

Internally, C++ may:

divide locations into chunks
run worker jobs
merge results deterministically
finalize the tick

GDScript should not manage dozens of simulation worker threads directly.

One practical migration strategy

Build the prototype so that the simulation is behind one interface even while it is still GDScript.

For example, conceptually:

SimulationBackend
├── submit_command()
├── step_tick()
├── get_current_tick()
├── get_date()
├── get_location_summary()
├── get_changed_locations()
└── compute_checksum()

Your first implementation is:

GDScriptSimulationBackend

Later you replace it with:

CppSimulationBackend

The UI and networking code should not care which implementation is active.

You do not need to make this an elaborate abstraction framework. Just avoid allowing every UI script to reach directly into simulation internals.

My recommendation for your current stage

Keep the whole skeleton in GDScript for now, including the fake simulation.

But structure it as though the simulation were already an external module:

one simulation owner
one command entry point
one tick entry point
read-only query functions
no UI mutation of simulation data

Then move pieces in roughly this order:

Core arrays and WorldState
Tick runner and calendar
Command application
Heavy economy/population systems
RNG and checksums
Multithreaded phases
Save-state serialization

Leave UI, prediction, scene handling, and network-session glue in GDScript.

The guiding rule is:

Prototype behavior in GDScript; move stable, authoritative, data-heavy systems to C++. Keep Godot-facing and rapidly changing systems in GDScript.

Is there a way to make adding new types of commands easier? Currently I've got an enum Type with 0 = none, 1 = pause, 2 = set speed, so on so forth. Or is this going to be my life for adding new ways to interact with the world? Basically I'm asking if I can make my job right now slightly harder to make my future life wayyy easier.

Yes. You can make your life much easier by stopping the enum from being the place where command behavior lives.

An enum is still useful as a stable serialized identifier, but it should only answer:

“What kind of command is this?”

It should not force one enormous match statement to know how every command works.

Recommended structure: command registry + handlers

Keep a generic command envelope:

class_name GameCommand
extends RefCounted

var type_id: int
var player_id: int
var sequence_id: int
var execution_tick: int
var payload: Dictionary

Then maintain a registry that maps each command type to a handler:

var command_handlers := {
    CommandType.PAUSE: PauseCommandHandler.new(),
    CommandType.SET_SPEED: SetSpeedCommandHandler.new(),
    CommandType.BUILD: BuildCommandHandler.new(),
}

The timeline does not care what the command means:

func execute_command(command: GameCommand, world: WorldState) -> void:
    var handler = command_handlers.get(command.type_id)

    if handler == null:
        push_error("Unknown command type")
        return

    handler.execute(command, world)

When adding a new interaction, you generally add:

A stable type ID.
A handler.
Its validation and execution behavior.
Its serialization rules.
Registration in the command registry.

You do not edit the central execution logic.

Give each command handler a common interface

Conceptually, each handler should support something like:

validate_submission(command, context)
validate_execution(command, world)
execute(command, world)
predict(command, presentation)
describe(command)

Not every handler must use every method, but having a shared shape gives you somewhere predictable to put each concern.

For example:

class_name CommandHandler
extends RefCounted

func validate_submission(command: GameCommand, context) -> bool:
    return true

func validate_execution(command: GameCommand, world: WorldState) -> bool:
    return true

func execute(command: GameCommand, world: WorldState) -> void:
    push_error("Command handler has no execute implementation")

func predict(command: GameCommand, prediction_state) -> void:
    pass

Then SetSpeedCommandHandler contains speed-specific behavior, while BuildCommandHandler contains building-specific behavior.

That prevents this:

match command.type_id:
    PAUSE:
        ...
    SET_SPEED:
        ...
    BUILD:
        ...
    MOVE_ARMY:
        ...
    CHANGE_TAX:
        ...
    # 140 more cases

A small dispatcher match is fine during prototyping. A giant one becomes painful.

Do not create a separate command class for every trivial variation

There are two extremes:

One generic command with a giant match

Easy now, painful later.

A unique class for absolutely everything
PauseCommand
UnpauseCommand
SetSpeedOneCommand
SetSpeedTwoCommand
SetSpeedThreeCommand

Very structured, but full of boilerplate.

For your game, I would use a middle ground:

one generic serialized GameCommand
one handler per meaningful command family
payload schemas defining the data needed by each type

For example, SET_SPEED can handle every speed value:

type_id: SET_SPEED
payload:
    speed: 3

You do not need separate types for every possible speed.

Similarly:

type_id: SET_POLICY
payload:
    country_id: 15
    policy_id: 7
    value: 2

One handler can cover a whole family of related settings.

Group commands by domain

You could organize them like:

commands/
├── command.gd
├── command_registry.gd
├── command_handler.gd
│
├── time/
│   ├── pause_command_handler.gd
│   └── set_speed_command_handler.gd
│
├── construction/
│   ├── build_command_handler.gd
│   ├── demolish_command_handler.gd
│   └── construction_commands.gd
│
├── military/
│   ├── assign_theater_command_handler.gd
│   └── set_doctrine_command_handler.gd
│
└── economy/
    ├── set_tax_command_handler.gd
    └── change_trade_policy_command_handler.gd

You might later discover that pause and unpause should both be one SET_PAUSED command. That is usually preferable:

SET_PAUSED:
    paused = true

rather than:

PAUSE
UNPAUSE

Commands should normally describe the desired authoritative change, not a UI gesture.

Separate command creation from command execution

A UI button should not manually assemble arbitrary dictionaries everywhere.

Instead, provide factories or helper functions:

CommandFactory.make_set_speed(player_id, speed)
CommandFactory.make_build(player_id, location_id, building_type)

The factory should:

choose the type ID
construct the correct payload
reject obviously malformed arguments
assign the player sequence number where appropriate

This means the UI does not need to know the internal serialized shape.

Without that, you may eventually have dozens of scripts doing slightly different versions of:

{
    "type": CommandType.BUILD,
    "location": selected_location,
    "building": building_id
}

A typo or outdated field name then becomes a runtime networking bug.

A factory provides one place to update the command’s format.

Give payloads explicit schemas

A raw Dictionary is convenient but dangerous because nothing prevents this:

payload["location"]
payload["location_id"]
payload["province"]

from all being used for the same concept.

During prototyping, a dictionary is fine. At minimum, define constants for payload fields:

const LOCATION_ID := &"location_id"
const BUILDING_TYPE_ID := &"building_type_id"

A more structured option is to use small payload classes:

class_name BuildCommandPayload
extends RefCounted

var location_id: int
var building_type_id: int

However, remember that whatever crosses the network ultimately needs a canonical serialized representation. Do not make serialization depend on arbitrary Godot object state.

A sensible progression is:

Prototype with dictionaries.
Standardize factories and field names.
Introduce typed payload structures where mistakes become common.
Serialize into packed integer/byte formats later.
Keep type IDs stable

Even with handlers, you still need some stable command identifier for:

networking
saves
replays
debugging
backward compatibility

An enum is good for this.

The important rule is: do not renumber existing command types after saves or replay logs depend on them.

This is dangerous:

enum Type {
    NONE,
    PAUSE,
    BUILD,
    SET_SPEED,
}

If inserting BUILD changes SET_SPEED from 2 to 3, old serialized commands could be misinterpreted.

Prefer explicit numbers:

enum Type {
    NONE = 0,
    SET_PAUSED = 1,
    SET_SPEED = 2,

    BUILD = 100,
    DEMOLISH = 101,

    SET_TAX = 200,
}

Ranges by domain are optional, but useful when inspecting logs.

Do not reuse a retired ID. Mark it deprecated.

Use registration instead of a central dependency pile

Your registry might expose:

func register_handler(type_id: int, handler: CommandHandler) -> void:
    assert(not handlers.has(type_id))
    handlers[type_id] = handler

Then each command module can be registered during startup.

That means adding a new command involves registering its handler rather than modifying several unrelated systems.

You still want registration to be deterministic. Do not derive multiplayer IDs from load order or dynamically generated hashes. The type ID should be explicitly defined and stable.

Separate authority from prediction

A useful split is:

Command handler
├── authoritative validation
├── authoritative execution
└── command metadata

Prediction handler
├── immediate visual response
└── reconciliation behavior

These can be methods on the same handler initially, but recognize that they act on different state.

For example, a build command’s authoritative execution changes construction data. Its prediction might only add a transparent building marker.

Do not let prediction call the authoritative execute() method against a fake copy unless you eventually intend to build full rollback prediction.

Consider commands versus simulation events

Do not use commands for every occurrence in the world.

A command represents an authoritative external decision:

player sets speed
player builds something
player changes a policy
host pauses
AI chooses an action

A simulation event is a deterministic result:

building completes
population grows
month ends
battle occurs
price changes

The completion of a building should generally not become a second network command. Every peer should derive it from the same state.

This distinction keeps the number of command types manageable.

A scalable pipeline

The structure you are aiming toward is approximately:

UI / AI
   ↓
Command factory
   ↓
Command submission
   ↓
Host submission validation
   ↓
Tick assignment
   ↓
Canonical command timeline
   ↓
Execution validation
   ↓
Registered command handler
   ↓
WorldState mutation
   ↓
Simulation consequences

New UI features then plug into the top, and new world interactions plug into the registry.

What I would build now

Since you are prototyping, avoid building a giant reflection-heavy framework. Add only enough structure to test whether it helps.

I would introduce:

GameCommand
CommandType with explicit numeric IDs
CommandHandler
CommandRegistry
CommandFactory
one handler for time control
one handler for a fake world action

You could combine pause and speed into a single time-control handler initially, although they should remain distinct serialized command types if their validation differs.

Then observe what adding the next five commands feels like.

A healthy result should look like:

Add enum ID
Add factory method
Add handler
Register handler
Add tests

It will never become completely automatic because every genuinely new interaction needs rules. The goal is that adding a command only requires defining its own rules, not editing the network system, tick runner, UI prediction system, replay system, and one enormous dispatcher separately.

The enum itself is not the problem. The problem is when the enum and central match statement become the architecture.

Sun, Jul 12 at 2:32 PM
Okay, next, how do I do a rigorous "predicted test value" system? This is just for UI elements really. Army orders & stuff like that might be more bespoke.

Treat a predicted UI value as a derived projection, not as a second authoritative value.

The basic formula is:

displayed value
=
confirmed simulation value
+
effects of locally pending commands

For example:

Confirmed treasury: 1,000
Pending build command: -200
Pending tax change: +50 projected
Displayed treasury: 850

The UI never writes 850 back into the simulation. It recalculates it from confirmed state plus pending effects.

The three layers

For each predictable UI value, distinguish:

Confirmed value
Pending command effects
Displayed predicted value

Conceptually:

confirmed_value: int
pending_effects: Array
predicted_value: int

But predicted_value should usually be recalculated, not treated as independently authoritative.

Give pending commands identities

Every locally issued command should already have something like:

player_id
local_sequence_id
command_type
payload

The prediction layer uses (player_id, local_sequence_id) as the prediction ID.

That lets you associate:

Command 42
→ treasury prediction: -200
→ construction capacity prediction: -1
→ location build-slot prediction: occupied

When the host accepts, rejects, or executes command 42, you know exactly which prediction to update.

Prediction lifecycle

A useful lifecycle is:

Created locally
Submitted
Accepted and scheduled
Executed authoritatively
Removed from prediction

And alternatively:

Created locally
Submitted
Rejected
Removed or corrected

You may want statuses such as:

enum PredictionStatus {
    LOCAL,
    SUBMITTED,
    SCHEDULED,
    EXECUTED,
    REJECTED
}

The UI may render them differently, but only the first few stages contribute pending effects.

Prediction records

Rather than having every UI element maintain arbitrary temporary arithmetic, create prediction records.

Conceptually:

PredictionRecord
- command ID
- affected value key
- operation
- amount
- status

Example:

command_id: 42
value_key: treasury
operation: add
amount: -200

Another command could affect several values:

Command 42:
- treasury: -200
- available_build_slots: -1
- projected_maintenance: +5

One command can therefore create multiple prediction effects.

Use stable value keys

You need a consistent way to identify predicted values.

For example:

country/12/treasury
country/12/army_manpower
location/84/build_slots
location/84/projected_food

You do not have to literally use strings. They could later become structured keys:

scope type
entity ID
value type

Such as:

PredictionKey(
    ScopeType.COUNTRY,
    12,
    PredictedValueType.TREASURY
)

For a prototype, strings or small records are fine. The important part is that UI elements and command predictors agree on the same identity.

Operations

For rigorous behavior, do not assume every prediction is simple addition.

You might need:

ADD
SET
MULTIPLY
RESERVE
REMOVE

Examples:

Treasury cost           → ADD -200
Set tax rate            → SET 15
Reserve army manpower   → ADD -5,000
Enable policy           → SET true

However, start with only the operations you actually need. Most resource and capacity predictions can be handled with additive deltas.

Ordering matters

Suppose the player issues:

Set tax rate to 10%
Set tax rate to 15%

You probably do not want both predictions applied independently.

For SET commands, later pending commands should override earlier ones according to canonical order:

confirmed value
→ pending command 51
→ pending command 52

The displayed value becomes the result after applying all pending commands in issuance order.

For additive effects:

confirmed 1,000
-200
-100
= 700

For sets:

confirmed speed 1
set speed 3
set speed 2
= speed 2

So prediction should reuse the same ordering principles as authoritative command execution.

Recalculate rather than undo

Avoid maintaining the predicted value by doing:

value -= 200
value += 200 when rejected

That becomes fragile with overlapping commands, reordered confirmations, and later state updates.

Instead:

predicted value =
confirmed base
then apply all still-pending effects in order

Whenever anything changes:

confirmed state changes
command is added
command is accepted
command is rejected
command executes

rebuild the prediction for affected keys.

This is much harder to corrupt.

Acceptance versus execution

Do not remove a prediction merely because the host accepted the command.

Suppose:

Command submitted at tick 100
Host schedules it for tick 105

From ticks 100–104, the authoritative treasury is still unchanged.

If you remove the prediction on acceptance, the UI briefly jumps backward.

So the prediction normally remains active until:

the authoritative state includes the command result

At tick 105:

simulation executes command
confirmed treasury changes
prediction record is removed
displayed value remains the same

That creates seamless reconciliation.

The reconciliation invariant

A successful prediction should satisfy:

Before execution:
confirmed 1,000 + prediction -200 = displayed 800

After execution:
confirmed 800 + no prediction = displayed 800

There should be no visual jump.

If the real effect differs:

Predicted cost: 200
Actual cost at execution: 220

Then reconciliation becomes:

Before: 800
After: 780

That correction is legitimate, though the UI should perhaps indicate why it changed.

Rejection

On rejection:

confirmed 1,000
prediction -200
displayed 800

Then remove the prediction:

confirmed 1,000
no prediction
displayed 1,000

You might show:

a brief rejection message
a red flash
tooltip explanation
command status icon

The prediction system itself should only remove the effect.

Base value updates while predictions exist

This is a crucial test.

Suppose:

Confirmed treasury: 1,000
Pending prediction: -200
Displayed: 800

Then a daily tax update changes confirmed treasury to 1,050.

The displayed value should become:

1,050 - 200 = 850

This is why predictions must sit on top of the latest confirmed value rather than copying the original base.

Prediction source

A command should ideally know how to describe its predicted UI effects.

Not necessarily in the authoritative handler itself, but through a related predictor.

Conceptually:

BuildCommand
Authoritative handler:
- validate
- execute

Prediction provider:
- calculate treasury reservation
- calculate slot reservation
- calculate projected maintenance

You might organize it as:

handler.get_prediction_effects(command, confirmed_state)

or:

prediction_registry.predict(command, confirmed_state)

Keeping prediction generation close to command semantics helps prevent duplicated rules.

Be cautious about values that can change before execution

Some values are safe to predict exactly:

selected tax rate
paused state
queued construction slot
fixed-price action cost
reserved manpower

Some are only estimates:

future market cost
projected revenue
dynamic construction time
diplomacy acceptance
army arrival time

Distinguish:

Exact pending value
Projected estimate

Do not visually present an estimate as though it is guaranteed.

You might use:

Treasury after queued orders: 800
Estimated monthly income: +42

Those are different semantics.

Reservations are often better than fake deductions

For resources like money, you may want the authoritative design itself to support reservations.

Instead of pretending the treasury has already fallen, display:

Treasury: 1,000
Reserved: 200
Available: 800

Then:

available = confirmed treasury - pending reservations

This is often clearer and avoids implying the transaction already occurred.

It also handles multiple pending commands naturally.

The UI could still prominently show 800 available, while a tooltip explains the 200 reservation.

Suggested prediction manager responsibilities

A central prediction manager should handle:

add predictions for command
remove predictions for command
mark command scheduled
mark command rejected
mark command executed
calculate predicted value for key
return pending records for UI explanations

The UI should ask:

What is the displayed treasury for country 12?
Why is it different from confirmed treasury?

It should not manually search command queues itself.

A small conceptual API

Not drop-in code, but the interface could look like:

prediction_manager.add_command(command)
prediction_manager.reject_command(command_id)
prediction_manager.complete_command(command_id)

prediction_manager.get_confirmed_value(key)
prediction_manager.get_predicted_value(key)
prediction_manager.get_effects(key)

get_effects(key) lets the tooltip explain:

Confirmed treasury: 1,000
Road construction: -200
Army recruitment: -150
Available after orders: 650

That explanatory capability is worth designing early.

Rigorous tests

The easiest way to test this is with a fake integer value.

Start with:

confirmed test value = 100

Then test these cases.

1. One additive prediction
Confirmed: 100
Pending: +10
Displayed: 110
2. Multiple additive predictions
Confirmed: 100
Pending: +10, -20, +5
Displayed: 95
3. Confirmed value changes while pending
Confirmed: 100
Pending: -20
Displayed: 80

Confirmed changes to 110
Displayed must become 90
4. Prediction executes exactly
Before execution:
Confirmed 100
Pending -20
Displayed 80

After execution:
Confirmed 80
Pending removed
Displayed remains 80
5. Prediction is rejected
Confirmed 100
Pending -20
Displayed 80

Reject command
Displayed returns to 100
6. Two set commands
Confirmed speed: 1
Pending set 3
Pending set 2
Displayed: 2

Reject the second command:

Displayed: 3

Execute the first:

Confirmed: 3
First prediction removed
Displayed remains 2 because second is still pending

That last scenario is especially valuable.

7. Out-of-order network responses

Issue commands 10 and 11.

Then receive:

Command 11 accepted
Command 10 rejected

The result should still be correct because predictions are identified individually.

8. Actual result differs from prediction
Confirmed: 100
Predicted cost: -20
Displayed: 80

Actual execution cost: -25
Confirmed becomes 75
Prediction removed
Displayed becomes 75

The system should correct cleanly without trying to “undo” 20 and then subtract 25 manually.

9. Save/load or UI reconstruction

Close and reopen the UI while a command is pending.

It should derive the same displayed value from:

confirmed state + pending command records

The UI should not need local hidden variables to remember what happened.

10. Zero predictions

Always verify:

no pending effects
→ displayed value equals confirmed value

That should be an invariant.

A useful debug panel

While building it, show:

Confirmed test value: 100
Predicted test value: 75

Pending effects:
#41 BUILD            -20
#42 RECRUIT          -10
#43 CANCEL_BUILD      +5

Also show each command’s status:

LOCAL
SUBMITTED
SCHEDULED FOR TICK 412
REJECTED
EXECUTED

This will make bugs obvious.

Recommended initial implementation

Build the first version around:

integer values
additive effects
explicit command IDs
one central prediction manager
recomputation from confirmed state
removal only after execution or rejection
explanatory effect lists

Then add SET operations once you test speed or policy commands.

Do not begin with an elaborate universal system for every possible UI behavior. Make a rigorous system for predictable scalar values, and let army paths, ghosts, and other spatial predictions have bespoke presentation systems that share only the same command-ID lifecycle.

The key invariant is:

A predicted value is always reproducible from current confirmed state and the ordered set of unresolved local command effects.