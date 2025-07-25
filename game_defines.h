#define GRAVITY_POWER 28
#define JUMP_POWER 6
#define MAX_SHAKE_TIMER 0.4f
#define EPSILON_VALUE 0.2f
#define WALK_SPEED 2
#define CIRCLE_RADIUS_MAX 10
#define SHOW_CIRCLE_DELAY 2
#define STAMINA_DRAIN_SPEED 0.1f
#define STAMINA_RECHARGE_SPEED 0.1f
#define WATER_ELEVATION 40 //NOTE: This is 62 in actual minecraft
#define BLOCK_SIZE 1 
#define AO_BIT_NOT_VISIBLE 61
#define AO_BIT_CREATING 62
#define AO_BIT_INVALID 63
#define SUB_SOIL_DEPTH 5
#define DISTANCE_CAN_PLACE_BLOCK 6 //NOTE: Minecraft has a reach distance of 5 in survival and 6 in creative
#define CAMERA_OFFSET make_float3(0, 0, 0)
#define ITEM_PER_SLOT_COUNT 64
#define ITEM_HOT_SPOTS 8
#define VOXEL_SIZE_IN_METERS 0.1f
#define VOXEL_SIZE_IN_METERS_SQR (VOXEL_SIZE_IN_METERS*VOXEL_SIZE_IN_METERS)
#define VOXELS_PER_METER 10
#define MAX_CONTACT_POINTS_PER_PAIR 300
#define PHYSICS_VELOCITY_SLEEP_THRESHOLD 0.1 //NOTE: Box2d is 0.05
#define PHYSICS_SLEEP_TIME_THRESHOLD 0.5 //NOTE: Seconds
#define PHYSICS_RESTITUTION_VELOCITY_THRESHOLD 0.1f //NOTE: metres per Seconds
#define PHYSICS_RESTITUTION_VELOCITY_THRESHOLD_SQR (PHYSICS_RESTITUTION_VELOCITY_THRESHOLD*PHYSICS_RESTITUTION_VELOCITY_THRESHOLD)
#define BOUNDING_BOX_MARGIN 0.3f
#define PHYSICS_ITERATION_COUNT 10
#define PHYSICS_TIME_STEP (1.0f / 60.0f)
#define TEST_EDGES_EDGES 0
#define SIMPLE_TERRAIN_HEIGHT 1
#define MAX_BUILDING_COUNT_PER_CHUNK 1

#define INVERSE_CHUNK_DIM_METRES 0.625 //NOTE: 1 / (CHUNK_DIM * VOXEL_SIZE_IN_METERS)
#define CHUNK_SIZE_IN_METERS 1.6 //NOTE: 16 * 0.1

//NOTE: DEBUGGING CONSTANTS
#define PERLIN_SIZE 128
#define MAX_BONES_PER_MODEL 1000