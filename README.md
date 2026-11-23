# ERL-HUB: Post-Earthquake Multi-Agency Emergency Relief Logistics Hub

Header-only, standard C++11/14/17 (no dependencies). `main.cpp` is a demo driver with 58 self-checks; exit code 0 means all passed.

## Build and run

    make run                 # default -std=c++14
    make STD=c++17 run       # or c++11
    make clean

## Layout

| File | Contents |
|---|---|
| `Common.hpp` | enums, parsers, binary I/O helpers, checksum, `IncidentCommander` forward declaration |
| `Exceptions.hpp` | `ReliefException : std::exception` plus `CargoOverweight`, `FuelDepletion`, `DamagedCorridor`, `InsufficientStock`, `DuplicateManifest`, `Storage` exceptions |
| `Manipulators.hpp` | `tabular`, `plain`, `serial`, `manifestHeader`, `manifestRule`, `indent(n)` |
| `Inventory.hpp` | `ReliefOps::Inventory`: `ReliefCargo`, `DistributionDepot`, overloaded `allocate(...)`, mission helpers |
| `Logistics.hpp` | `ReliefOps::Logistics`: inline utils, `GPSBeacon`, `EmergencyLifeSupportUnit`, `TransportVehicle`, `ReliefVehicle` hierarchy, `FleetRegistry` |
| `DispatchQueue.hpp` | `PriorityDispatchQueue<T, Compare>`, urgency functors |
| `Persistence.hpp` | `AuditStore` (binary, checksummed, atomic replace via temp file + rename) |
| `IncidentCommander.hpp` | friend of `ReliefCargo` and `DistributionDepot` |

## Syllabus traceability

**Unit I**
- Namespaces `ReliefOps::Logistics` / `ReliefOps::Inventory`.
- Three `allocate` overloads (volume payload, triage, mission parameters).
- Call/return by reference: `stockRef`, `escalate`, `consumePayload`, `MissionParams&`.
- Inline `haversineKm`, `payloadKg`, `etaHours`.
- `friend class IncidentCommander` overrides depot reserves and cargo priority.

**Unit II**
- ADTs with private state: `ReliefCargo`, `DistributionDepot`, `TransportVehicle`.
- Statics: `ReliefCargo::liveManifests()`, `TransportVehicle::operationalVehicles()` and `totalDispatchedTonnes()`.
- Dynamic constructor, deep copy constructor, copy-and-swap assignment, destructors (heap id/lot arrays, `ReliefCargo**` manifest).
- `+=` restock, `-=` departure deduction, `==` manifest identity / duplicate detection, `<<` and `>>` (plain, tabular and serial modes).

**Unit III**
- `ReliefVehicle` derives into `AerialTransport`, `GroundConvoy`, `AmphibiousCraft`, all via `virtual public`.
- Hybrid `AmphibiousMedicalUnit` inherits two of them and holds a single `ReliefVehicle`.
- Containership: `GPSBeacon` and `EmergencyLifeSupportUnit` are members.
- `evaluateRoute` is called through `ReliefVehicle*`; the destructor trace shows the full virtual destructor chain.

**Unit IV**
- Exception hierarchy under `std::exception`.
- Atomic binary audit file with magic, version and FNV-1a checksum; a corrupted file is rejected and the caller's data stays untouched.
- `PriorityDispatchQueue<T>`, `std::vector`, `std::unordered_map`, functors `LessUrgent` / `MoreUrgent` / `CargoPriorityFirst`, user-defined manipulators.

## Notes
- POSIX `rename()` is atomic; on Windows the code falls back to remove + rename, which is not atomic.
- Binary files are written in native byte order (not portable across endianness).
