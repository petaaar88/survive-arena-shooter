# CLAUDE.md — "Survive" Arena Shooter

## Project Overview
A first-person timed survival shooter built with Irrlicht Engine and IrrKlang audio. Player must survive 5 minutes against waves of melee enemies in an arena. Written in C++.

## Build & Run (Visual Studio 2022)
```
Solution: Survive.sln
Project:  Survive.vcxproj
Config:   Debug/Release x64

Dependencies:
  - Irrlicht 1.8+
  - IrrKlang 1.6+
  - Bullet Physics 3.x (btBulletDynamicsCommon.h)

VS2022 Project Setup:
  1. Project → Properties → C/C++ → Additional Include Directories:
     $(SolutionDir)lib\irrlicht\include;
     $(SolutionDir)lib\irrklang\include;
     $(SolutionDir)lib\bullet\include;

  2. Project → Properties → Linker → Additional Library Directories:
     $(SolutionDir)lib\irrlicht\lib;
     $(SolutionDir)lib\irrklang\lib;
     $(SolutionDir)lib\bullet\lib;

  3. Linker → Input → Additional Dependencies:
     Irrlicht.lib; irrKlang.lib;
     BulletDynamics.lib; BulletCollision.lib; LinearMath.lib;

  4. Copy DLLs to output directory (Post-Build Event):
     xcopy /y "$(SolutionDir)lib\irrlicht\bin\Irrlicht.dll" "$(OutDir)"
     xcopy /y "$(SolutionDir)lib\irrklang\bin\irrKlang.dll" "$(OutDir)"

Working directory: $(ProjectDir) (so assets/ is found correctly)
```

## Project Structure
```
Survive/
├── Survive.sln
├── Survive.vcxproj
├── CLAUDE.md
├── lib/
│   ├── irrlicht/        # include/, lib/, bin/
│   ├── irrklang/        # include/, lib/, bin/
│   └── bullet/          # include/, lib/
├── assets/
│   ├── models/          # Q2 .md2 player models + skins
│   ├── maps/            # Level geometry (BSP or custom)
│   ├── textures/        # HUD textures, crosshair, pickup icons
│   └── sounds/          # .wav/.ogg sound effects
├── src/
│   ├── main.cpp         # Entry point, creates Game instance
│   ├── Game.h/cpp       # Main loop, game state machine
│   ├── GameObject.h/cpp # Base class: Irrlicht node + Bullet body pair
│   ├── Player.h/cpp     # FPS player: movement, shooting, health, ammo
│   ├── Enemy.h/cpp      # Single enemy: AI, animation, damage
│   ├── EnemyManager.h/cpp  # Spawning, wave logic, difficulty scaling
│   ├── Pickup.h/cpp     # Health and ammo pickups with respawn
│   ├── HUD.h/cpp        # 2D overlay: health, ammo, timer, crosshair, kills
│   ├── Physics.h/cpp    # Bullet physics world wrapper
│   ├── SoundManager.h/cpp  # IrrKlang wrapper for all audio
│   └── InputHandler.h/cpp  # IEventReceiver for keyboard + mouse
```

## Architecture

### Game States
```
MENU → PLAYING → WIN (timer reaches 0)
                → GAMEOVER (health reaches 0)
WIN/GAMEOVER → MENU (restart)
```

### Main Loop (Game.cpp)
```
while device->run():
    1. Calculate deltaTime
    2. Handle input (InputHandler)
    3. Update game state:
       - MENU: wait for start
       - PLAYING:
           a. Update player (movement, shooting)
           b. Update enemies (AI, animation)
           c. Step Bullet physics (world->stepSimulation)
           d. Sync Bullet transforms → Irrlicht nodes
           e. Check pickup triggers (ghost object overlaps)
           f. Update timer, HUD
       - WIN/GAMEOVER: wait for restart/quit
    4. Render scene (smgr->drawAll)
    5. Draw HUD overlay (2D)
```

### Class Responsibilities

**GameObject** — Base class for anything that exists in the world. Pairs an Irrlicht `ISceneNode*` with a Bullet `btRigidBody*` (or `btGhostObject*`). Handles:
- Position/rotation sync between Bullet and Irrlicht each frame
- Alive/dead flag and `markForRemoval()`
- Virtual `update(float deltaTime)` for subclass logic
- Creation and cleanup of both the scene node and physics body

```cpp
class GameObject {
protected:
    scene::ISceneNode*  m_node;       // Irrlicht visual
    btRigidBody*        m_body;       // Bullet physics (nullptr for triggers)
    btGhostObject*      m_ghost;      // Bullet trigger (nullptr for physics objects)
    bool                m_alive;
    bool                m_removeMe;

public:
    GameObject(scene::ISceneNode* node, btRigidBody* body);
    GameObject(scene::ISceneNode* node, btGhostObject* ghost);
    virtual ~GameObject();

    virtual void update(float dt) = 0;
    void syncPhysicsToNode();         // copy Bullet transform → Irrlicht node

    core::vector3df getPosition() const;
    void setPosition(const core::vector3df& pos);

    bool isAlive() const        { return m_alive; }
    bool shouldRemove() const   { return m_removeMe; }
    void markForRemoval()       { m_removeMe = true; }
    void kill()                 { m_alive = false; }

    scene::ISceneNode* getNode()  { return m_node; }
    btRigidBody*       getBody()  { return m_body; }
};
```

**Game** — Owns all other objects. Manages Irrlicht device, scene manager, game state enum. Runs the main loop. Transitions between MENU/PLAYING/WIN/GAMEOVER.

**Player** *(extends GameObject)* — Wraps Irrlicht FPS camera. Handles WASD movement, mouse look, jumping. Manages health (100 max) and ammo. Shooting via raycasting from camera into scene. Takes damage from enemies. Uses kinematic Bullet body.

**Enemy** *(extends GameObject)* — Wraps an `IAnimatedMeshSceneNode` loaded from .md2. Simple state machine:
- IDLE: standing still (just spawned)
- CHASE: move toward player position each frame
- ATTACK: close enough to player, deal damage on cooldown
- DEAD: play death animation, remove after delay

Movement is direct vector toward player (no pathfinding needed). Uses dynamic Bullet capsule body.

**EnemyManager** — Holds a list of active Enemy pointers. Spawns enemies at predefined spawn points on a timer. Controls wave difficulty based on elapsed time:
- 0:00–1:00 → max 3 alive, spawn every 5s
- 1:00–3:00 → max 6 alive, spawn every 3s
- 3:00–5:00 → max 10 alive, spawn every 2s

Removes dead enemies from the list after death animation finishes. Calls `enemy->shouldRemove()` to clean up.

**Pickup** *(extends GameObject)* — A scene node at a fixed position. Two types: HEALTH (+25 HP) and AMMO (+30 rounds). Uses `btGhostObject` for trigger overlap detection. Disappears on collect, respawns after 30 seconds.

**HUD** — Uses Irrlicht's 2D drawing functions (`draw2DRectangle`, `draw2DImage`, `IGUIFont`). Draws: health bar (top-left), ammo count (bottom-right), timer countdown (top-center), crosshair (center), kill count (top-right). Draws menu/win/gameover screens as 2D overlays.

**Physics** — Wraps Bullet physics world. Creates and steps `btDiscreteDynamicsWorld` each frame. Manages rigid bodies for: player (btCapsuleShape), enemies (btCapsuleShape), level geometry (btBvhTriangleMeshShape from map mesh), pickups (btGhostObject + btSphereShape as trigger). Provides helper methods to create/remove bodies and sync Bullet transforms ↔ Irrlicht scene nodes.

**SoundManager** — Initializes IrrKlang engine. Loads sounds on startup. Exposes simple methods: `playShoot()`, `playEnemyHurt()`, `playEnemyDeath()`, `playPlayerHurt()`, `playPickup()`, `playWin()`, `playLose()`, `startAmbient()`, `stopAmbient()`.

**InputHandler** — Implements `IEventReceiver`. Tracks key states in a bool array. Tracks mouse button state and mouse delta for look. Provides `isKeyDown(KEY)`, `isMouseDown()`, `getMouseDelta()`.

## Key Game Values
```
PLAYER_HEALTH        = 100
PLAYER_SPEED         = 200.0f    // units per second
PLAYER_AMMO_START    = 100
GUN_DAMAGE           = 25
GUN_FIRE_RATE        = 0.15f     // seconds between shots
GAME_DURATION        = 300.0f    // 5 minutes in seconds

ENEMY_BASIC_HEALTH   = 50
ENEMY_BASIC_SPEED    = 80.0f
ENEMY_BASIC_DAMAGE   = 10
ENEMY_ATTACK_RANGE   = 30.0f
ENEMY_ATTACK_COOLDOWN = 1.0f     // seconds

ENEMY_FAST_HEALTH    = 30
ENEMY_FAST_SPEED     = 150.0f
ENEMY_FAST_DAMAGE    = 5

PICKUP_HEALTH_AMOUNT = 25
PICKUP_AMMO_AMOUNT   = 30
PICKUP_RESPAWN_TIME  = 30.0f     // seconds
```

## MD2 Animation Frames (Quake 2 Standard)
```
STAND       =   0 –  39
RUN         =  40 –  45
ATTACK      =  46 –  53
PAIN_A      =  54 –  57
PAIN_B      =  58 –  61
PAIN_C      =  62 –  65
JUMP        =  66 –  71
CROUCH_STAND = 135 – 153
CROUCH_WALK  = 154 – 159
DEATH_A     = 178 – 183
DEATH_B     = 184 – 189
DEATH_C     = 190 – 197
```

## Shooting (Raycast)
```cpp
// Get ray from camera center into scene
core::line3d<f32> ray;
ray.start = camera->getPosition();
ray.end = ray.start + (camera->getTarget() - ray.start).normalize() * 1000.0f;

// Check collision with enemy triangle selectors
scene::ISceneCollisionManager* collMan = smgr->getSceneCollisionManager();
core::vector3df hitPoint;
core::triangle3df hitTriangle;
scene::ISceneNode* hitNode = collMan->getSceneNodeAndCollisionPointFromRay(
    ray, hitPoint, hitTriangle);

// If hitNode belongs to an enemy, deal damage
```

## Enemy AI (per frame)
```
if state == IDLE:
    if distance_to_player < 500: state = CHASE

if state == CHASE:
    direction = normalize(player_pos - enemy_pos)
    move(direction * speed * deltaTime)
    set animation RUN
    if distance_to_player < ATTACK_RANGE: state = ATTACK

if state == ATTACK:
    set animation ATTACK
    if attack_cooldown_elapsed:
        deal damage to player
        reset cooldown
    if distance_to_player > ATTACK_RANGE: state = CHASE

if state == DEAD:
    play death animation once
    after animation ends: mark for removal
```

## Bullet Physics Setup (Physics.cpp)
```cpp
// Initialize Bullet world
btDefaultCollisionConfiguration* collisionConfig = new btDefaultCollisionConfiguration();
btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfig);
btBroadphaseInterface* broadphase = new btDbvtBroadphase();
btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();
btDiscreteDynamicsWorld* world = new btDiscreteDynamicsWorld(
    dispatcher, broadphase, solver, collisionConfig);
world->setGravity(btVector3(0, -9.81f, 0));
```

## Bullet Rigid Bodies
```cpp
// Player — kinematic capsule
btCapsuleShape* playerShape = new btCapsuleShape(0.5f, 1.8f);
btRigidBody* playerBody = createRigidBody(0.0f, playerShape, startPos);
playerBody->setCollisionFlags(
    playerBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);

// Enemy — dynamic capsule
btCapsuleShape* enemyShape = new btCapsuleShape(0.5f, 1.5f);
btRigidBody* enemyBody = createRigidBody(10.0f, enemyShape, spawnPos);
enemyBody->setAngularFactor(btVector3(0, 0, 0)); // no tipping over

// Map — static triangle mesh
btTriangleMesh* triMesh = new btTriangleMesh();
// Fill triMesh from Irrlicht mesh buffers
btBvhTriangleMeshShape* mapShape = new btBvhTriangleMeshShape(triMesh, true);
btRigidBody* mapBody = createRigidBody(0.0f, mapShape, btVector3(0,0,0));

// Pickup — ghost object (trigger, no physics response)
btSphereShape* pickupShape = new btSphereShape(1.0f);
btGhostObject* ghost = new btGhostObject();
ghost->setCollisionShape(pickupShape);
ghost->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
world->addCollisionObject(ghost);
```

## Syncing Bullet ↔ Irrlicht (each frame)
```cpp
// Step physics
world->stepSimulation(deltaTime, 10);

// Sync each enemy's Bullet body → Irrlicht node
btTransform transform;
enemyBody->getMotionState()->getWorldTransform(transform);
btVector3 pos = transform.getOrigin();
enemyNode->setPosition(core::vector3df(pos.getX(), pos.getY(), pos.getZ()));
```

## Notes
- All timing uses `ITimer::getTime()` for delta time calculations
- Bullet handles all physics: player-wall, enemy-wall, enemy-enemy collisions
- Pickups use `btGhostObject` as triggers (no physics response, just overlap detection)
- Shooting still uses Irrlicht raycasting (or Bullet `btCollisionWorld::rayTest`)
- No pathfinding — enemies apply force/velocity toward the player via Bullet
- Sync direction: Player input → Bullet body → Irrlicht node (for enemies); Irrlicht camera → Bullet kinematic body (for player)
- Keep all game constants in a single header or at the top of Game.h for easy tuning
- Bullet uses Y-up coordinate system (same as Irrlicht default)
