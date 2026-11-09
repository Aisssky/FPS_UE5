# VANTYX
### An Open-Source Multiplayer Tactical Shooter Project Built with Unreal Engine 5

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-black?logo=unrealengine)
![Language](https://img.shields.io/badge/Language-C%2B%2B-blue)
![Project Type](https://img.shields.io/badge/Project-Open%20Source-green)
![Purpose](https://img.shields.io/badge/Purpose-Learning%20%26%20Portfolio-orange)

VANTYX is an independent, open-source multiplayer tactical shooter project built with **Unreal Engine 5.8 and C++**.

Inspired by tactical competitive shooters such as VALORANT, VANTYX explores the gameplay systems and engineering challenges behind team-based multiplayer shooters. Its development focuses on server-authoritative combat, networked weapon mechanics, lag compensation, the Gameplay Ability System, and round-based team gameplay.

The project is developed as a **learning, experimentation, and portfolio project**, with its source code shared publicly to help other developers explore Unreal Engine multiplayer game development.

Rather than attempting to build a large-scale commercial game, VANTYX focuses on a compact and extensible tactical shooter experience backed by well-structured gameplay systems.

---

## Table of Contents

- [Project Vision](#project-vision)
- [Core Features](#core-features)
- [Technical Highlights](#technical-highlights)
- [Architecture Overview](#architecture-overview)
- [Gameplay Systems](#gameplay-systems)
- [Lyra Integration and Learning References](#lyra-integration-and-learning-references)
- [Technical Design Principles](#technical-design-principles)
- [Development Environment](#development-environment)
- [Getting Started](#getting-started)
- [Project Structure](#project-structure)
- [Branch Structure](#branch-structure)
- [Acknowledgements](#acknowledgements)
- [Third-Party Assets and Licensing](#third-party-assets-and-licensing)
- [Disclaimer](#disclaimer)

---

## Project Vision

VANTYX aims to explore the foundations of a competitive multiplayer tactical shooter through four interconnected gameplay pillars.

### Tactical Gunplay

Gunplay is built around responsive weapon behavior and authoritative server-side combat resolution.

The design emphasizes weapon accuracy, recoil, movement-dependent shooting behavior, ammunition management, and reliable hit registration under network latency.

### Ability-Driven Gameplay

The Gameplay Ability System provides a foundation for character attributes, gameplay effects, and tactical abilities.

Inspired by ability-driven tactical shooters, the project explores how abilities can complement traditional weapon combat without compromising server authority.

### Team-Based Competitive Combat

The gameplay framework is designed around team-based engagements and round-based match progression.

Its intended gameplay structure includes team coordination, economy management, objective-based rounds, and tactical decision-making.

### Multiplayer Engineering

Multiplayer correctness is a central engineering goal.

The project explores server-authoritative gameplay, client-server communication, historical hitbox validation, time synchronization, and network condition testing.

---

## Core Features

### Multiplayer Combat

- Server-authoritative shooting requests.
- Server-side hit validation.
- Weapon switching and aiming.
- Firing and reloading.
- Magazine and reserve ammunition management.
- Weapon pickups.
- Damage and elimination handling.
- Player death and respawn flow.

### Weapon Mechanics

- Data-driven weapon configuration.
- Movement-dependent weapon accuracy.
- Counter-strafe accuracy behavior.
- Server-side weapon spread calculation.
- Deterministic recoil pattern design.
- Fire rate validation.
- Client-side crosshair feedback.
- Authoritative hit confirmation.

### Gameplay Ability System

- Custom Ability System Component.
- Custom Attribute Set.
- Gameplay Abilities and Gameplay Effects.
- Ability costs and cooldowns.
- Target data workflows.
- Tactical ability framework.
- Gameplay Cue-based combat and ability presentation.

### Tactical Abilities

The ability framework explores tactical gameplay mechanics such as:

- Smoke-based visibility obstruction.
- Flash-based visual effects.
- Ability activation and cooldown management.
- Server-authoritative ability validation.
- Replicated ability-related gameplay effects.

### Round-Based Team Gameplay

- Team assignment and team state.
- Round state management.
- Buy Phase and economy framework.
- Objective-based gameplay architecture.
- Round-based respawn rules.
- Spectating and match flow.

The overall design targets a compact tactical shooter experience that can be extended with additional weapons, abilities, objectives, and match rules.

---

## Technical Highlights

### 1. Server-Authoritative Combat Architecture

Combat decisions are validated and resolved by the server.

The shooting pipeline is designed to prevent clients from directly determining authoritative hit results or damage.

Core data structures:

- `FShotRequest`
- `FShotResult`
- `EShotRejectReason`

Core combat component:

- `UCombatComponent`

The shooting workflow separates client-side shooting intent from server-side combat validation.

The server is responsible for:

- Validating shot requests.
- Checking shot sequence and duplicate requests.
- Enforcing fire rate constraints.
- Validating shooting origin and timestamp information.
- Managing authoritative ammunition consumption.
- Performing hit detection.
- Applying damage and returning shot results.

The client does not submit `HitActor`, `FHitResult`, `Damage`, or `BoneName` as authoritative combat results.

This keeps hit validation and damage decisions within the server-controlled gameplay pipeline.

### 2. Lag Compensation and Server-Side Rewind

VANTYX explores historical hitbox recording and server-side rewind to improve shooting accuracy under network latency.

Core components:

- `UNetworkTimeSubsystem`
- `ULagCompensationComponent`
- `UHitValidationComponent`

The system includes:

- Historical hitbox frame recording.
- Dedicated hitboxes for head, torso, and pelvis regions.
- Historical frame interpolation.
- Server-side rewind traces.
- Configurable rewind windows.
- Fallback validation when historical data is unavailable.

The server uses historical collision information to evaluate shooting requests against an appropriate past state of the target.

The architecture also provides a foundation for extending historical validation to dynamic gameplay obstruction.

### 3. Gameplay Ability System Integration

GAS is used as the foundation for character attributes, abilities, and gameplay effects.

Core classes and assets include:

- `UFPSAbilitySystemComponent`
- `UFPSAttributeSet`
- `GA_FireWeapon`
- `GA_Reload`
- `GE_Damage`

The Attribute Set manages:

- Health
- MaxHealth
- Stamina
- MaxStamina

The Ability System Component is hosted by the PlayerState to support its intended lifecycle across character respawns.

Weapon ammunition, reserve ammunition, and score are managed by their respective gameplay systems rather than being placed in the Attribute Set.

Weapon firing and reloading abilities retain a lightweight `LocalOnly` wrapper responsibility. The authoritative shooting pipeline remains within the combat architecture instead of moving the entire firing loop into Gameplay Abilities.

### 4. Network Time Synchronization

`UNetworkTimeSubsystem` provides client-side server time estimation for networked gameplay systems.

This supports time-dependent combat validation and historical hitbox evaluation.

The time synchronization design is particularly important for coordinating client shooting requests with server-side rewind calculations.

### 5. Data-Driven Weapon Configuration

Weapon configuration is organized around the existing `UWeaponData` data asset.

It provides a centralized foundation for weapon-specific configuration, including grip points, first-person and third-person animation references, and extensible accuracy and recoil data.

This approach helps keep weapon-specific behavior configurable without introducing unnecessary parallel configuration systems.

### 6. Round-Based Match Architecture

The match framework separates server-side match rules from replicated match state.

Core classes include:

- `AShooterGameModeBase`
- `AShooterGameStateBase`
- PlayerState-based player and team information.

The architecture is designed to support a round-based gameplay flow:

1. Warmup
2. Buy Phase
3. Round Active
4. Round End
5. Match End

The framework provides a foundation for team-based rounds, economy management, objective gameplay, and round-specific respawn rules.

---

## Architecture Overview

VANTYX separates gameplay responsibilities into several cooperating systems.

| System | Main Responsibility |
|---|---|
| Character | Player movement, character state, and interaction |
| Weapon | Weapon behavior, configuration, and presentation |
| Combat | Shooting requests, ammunition, and combat resolution |
| Ability System | Gameplay Abilities, Attributes, and Gameplay Effects |
| Lag Compensation | Historical hitbox recording and rewind evaluation |
| Hit Validation | Server-side hit detection and validation |
| PlayerState | Player-related persistent state and Ability System ownership |
| GameMode | Server-side match rules and respawn requests |
| GameState | Replicated match state and scoreboard information |
| Network Time | Client-side estimation of server time |
| Presentation | Gameplay Cues and combat feedback |

The architecture emphasizes clear system boundaries.

For example, weapon behavior, shooting request validation, damage application, and visual presentation are related but separate responsibilities. This makes it easier to extend gameplay features while preserving the existing authoritative combat pipeline.

---

## Gameplay Systems

### Combat Flow

The combat system follows a server-authoritative request and response model.

A simplified shooting workflow:

```text
Client Input
    |
    v
Build Shot Request
    |
    v
Client Sends FShotRequest
    |
    v
Server Validates Request
    |
    +---- Rejected
    |       |
    |       v
    |   Client Shot Rejected
    |
    +---- Accepted
            |
            v
     Server Hit Validation
            |
            v
     Authoritative Damage
            |
            v
       FShotResult
            |
            v
      Client Feedback
```

This separates player input from authoritative combat decisions.

### Ability Flow

The ability framework uses GAS to organize ability activation, attribute changes, gameplay effects, and tactical ability behavior.

The design aims to keep ability costs, cooldowns, targeting, server validation, and visual presentation within clearly defined responsibilities.

### Match Flow

The match framework is designed around team-based rounds.

The intended structure separates:

- Match-level state.
- Round-level state.
- Player and team information.
- Economy and purchasing.
- Objective progression.
- Respawn and spectating behavior.

This provides a foundation for expanding the project toward a more complete tactical competitive gameplay loop.

---

## Lyra Integration and Learning References

VANTYX uses Epic Games' **Lyra Starter Game** as an important learning and development reference.

Lyra provides valuable examples of Unreal Engine gameplay architecture, including:

- Gameplay Ability System integration.
- Modular gameplay architecture.
- Gameplay Effects and Gameplay Cues.
- Data-driven gameplay design.
- Multiplayer-oriented gameplay patterns.

Throughout development, VANTYX selectively studies, adapts, and integrates suitable Lyra concepts and resources into its own architecture.

The project does not aim to reproduce Lyra or completely replace its existing systems.

Instead, it combines selected Lyra concepts with independently developed gameplay systems, particularly its server-authoritative shooting pipeline and lag compensation architecture.

This approach provides an opportunity to understand established Unreal Engine patterns while making independent architectural decisions based on the project's gameplay requirements.

For the original project and its resources, refer to the official [Lyra Starter Game distribution](https://www.unrealengine.com/marketplace/en-US/product/lyra).

---

## Technical Design Principles

### Server Authority First

The server owns authoritative gameplay decisions, including hit validation, damage application, and combat state changes.

### Separation of Responsibilities

Weapon logic, combat requests, abilities, attributes, and match rules are organized around distinct responsibilities.

### Extend Existing Systems

New features should extend established project systems where appropriate instead of introducing unnecessary duplicate frameworks.

### Data-Driven Configuration

Weapon and gameplay configuration should remain extensible and maintainable through appropriate Unreal Engine data assets and configuration systems.

### Reproducible Multiplayer Testing

Multiplayer features should be evaluated in dedicated server and multiple-client environments.

Testing should consider latency, packet loss, timing differences, and gameplay edge cases rather than relying on compilation success alone.

### Practical Engineering Over Unnecessary Complexity

The project prioritizes practical multiplayer gameplay architecture and a focused tactical shooter experience over large-scale infrastructure that is outside its intended scope.

---

## Development Environment

| Component | Version / Technology |
|---|---|
| Game Engine | Unreal Engine 5.8 |
| Programming Language | C++ |
| Ability Framework | Gameplay Ability System |
| Multiplayer Model | Client-Server |
| Combat Model | Server-Authoritative |
| Development Environment | Rider |
| Target Platform | Windows |

---

## Getting Started

### Prerequisites

Before opening the project, make sure the following tools are installed:

- Unreal Engine 5.8.
- Visual Studio 2022 with Unreal Engine C++ development support.
- Git.
- Any required third-party project dependencies and licensed assets.

### Clone the Repository

```bash
git clone https://github.com/Aisssky/FPS_UE5.git
```

### Open the Project

1. Clone the repository.
2. Open the project `.uproject` file with Unreal Engine 5.8.
3. Generate project files if necessary.
4. Build the C++ project using Visual Studio.
5. Launch the project through Unreal Editor.

### Dedicated Server Testing

The project supports dedicated server-oriented multiplayer testing.

An example server launch command:

```bash
UnrealEditor.exe FPS.uproject /Game/Maps/FPSMap -server -log
```

Additional client instances can be launched with the appropriate connection settings and different player names.

For example, use different `?Name=` parameters for separate client instances.

Some third-party assets or dependencies may need to be obtained separately according to their respective licensing terms.

---

## Project Structure

The project is organized around Unreal Engine's standard project structure and its gameplay modules.

The major architectural areas include:

- Character and player state.
- Weapon and combat systems.
- Gameplay Ability System integration.
- Lag compensation and hit validation.
- Match and round management.
- Network time synchronization.
- Gameplay presentation and effects.

For implementation details, refer to the corresponding source files and project documentation.

---

## Branch Structure

The repository maintains separate development branches for the original foundation and subsequent architectural extensions.

| Branch | Description |
|---|---|
| `main` | Original multiplayer FPS foundation developed through tutorial-based learning |
| `feature/gas-migration` | GAS integration, combat architecture improvements, and multiplayer gameplay extensions |

The GAS migration branch builds upon the original foundation rather than representing an unrelated project.

---

## Acknowledgements

### DruidMech — UE5 Multiplayer FPS Tutorial

The initial multiplayer FPS foundation was developed through learning and experimentation based on the tutorial project by DruidMech.

Original repository:

[UE5 Multiplayer FPS](https://github.com/DruidMech/UE5_Multiplayer_FPS)

VANTYX is an independent learning and development project. It is not the original tutorial repository.

The subsequent architecture, gameplay systems, and extensions are developed independently as part of the project's ongoing learning and engineering work.

### Epic Games — Lyra Starter Game

Special thanks to Epic Games for providing the Lyra Starter Game as a valuable learning resource.

Lyra has been an important reference for understanding Unreal Engine gameplay architecture, GAS, modular systems, and multiplayer gameplay design.

### Unreal Engine Community

Thanks to the Unreal Engine developer community for sharing technical knowledge, examples, and resources that help make game development more accessible.

---

## Third-Party Assets and Licensing

VANTYX is intended to be an open-source learning project. However, the repository's own source code and independently created resources are separate from third-party content included or referenced by the project.

Third-party assets and resources may be subject to their own license terms.

In particular:

- Lyra Starter Game content remains subject to Epic Games' applicable licensing terms.
- Third-party assets are not automatically covered by the VANTYX repository license.
- Resources that cannot be redistributed in source format should be obtained through their original authorized distribution channels.
- Modified third-party resources remain subject to the applicable terms of their original content.
- Asset sources and setup requirements should be documented where applicable.

Before using or redistributing any third-party content, review the relevant license and distribution conditions.

The project aims to make its original code and learning materials publicly accessible while respecting the ownership and licensing requirements of external resources.

---

## Disclaimer

VANTYX is an independent, non-commercial, open-source learning and portfolio project.

- This project is inspired by tactical shooters such as VALORANT.
- VANTYX is not affiliated with, endorsed by, or associated with Riot Games.
- VALORANT and related trademarks belong to their respective owners.
- Unreal Engine and Lyra Starter Game are products of Epic Games.
- Third-party assets and resources remain the property of their respective owners and are subject to their applicable license terms.

The project is developed for educational, research, and portfolio demonstration purposes.

---

## Author

Developed independently as an Unreal Engine multiplayer game development learning project.

**Areas of Focus**

- Unreal Engine C++ Development
- Multiplayer Gameplay Programming
- Server-Authoritative Combat Architecture
- Gameplay Ability System
- Networked Weapon Mechanics
- Lag Compensation and Hit Validation
- Round-Based Gameplay Systems

VANTYX is an ongoing exploration of multiplayer tactical shooter development in Unreal Engine.

If you are interested in Unreal Engine multiplayer programming, feel free to explore the source code, study the architecture, and learn from the implementation.

**Thanks for checking out VANTYX!**
