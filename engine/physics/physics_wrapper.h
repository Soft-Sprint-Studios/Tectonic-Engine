/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef PHYSICS_WRAPPER_H
#define PHYSICS_WRAPPER_H

//----------------------------------------//
// Brief: Physics wrapper written in C++ using bullet
//----------------------------------------//

#include "math_lib.h"
#include <stdbool.h>
#include "physics_api.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		COL_NOTHING = 0,
		COL_STATIC = 1 << 0,
		COL_PLAYER = 1 << 1,
		COL_DYNAMIC = 1 << 2,
		COL_ALL = -1
	} CollisionGroup;

	typedef struct PhysicsWorld* PhysicsWorldHandle;
	typedef struct RigidBody* RigidBodyHandle;

	typedef struct {
		bool hasHit;
		Vec3 point;
		Vec3 normal;
		RigidBodyHandle hitBody;
	} RaycastHitInfo;

	PHYSICS_API PhysicsWorldHandle Physics_CreateWorld(float gravity_y);
	PHYSICS_API void Physics_DestroyWorld(PhysicsWorldHandle world);
	PHYSICS_API void Physics_StepSimulation(PhysicsWorldHandle world, float deltaTime);

	PHYSICS_API RigidBodyHandle Physics_CreatePlayerCapsule(PhysicsWorldHandle world, float radius, float height, float mass, Vec3 startPos);
	PHYSICS_API RigidBodyHandle Physics_CreateStaticTriangleMesh(PhysicsWorldHandle world, const float* vertices, int numVertices, const unsigned int* indices, int numIndices, Mat4 transform, Vec3 scale);
	PHYSICS_API RigidBodyHandle Physics_CreateDynamicConvexHull(PhysicsWorldHandle world, const float* points, int numPoints, float mass, Mat4 transform);
	PHYSICS_API RigidBodyHandle Physics_CreateDynamicBrush(PhysicsWorldHandle world, const float* vertices, int numVertices, float mass, Mat4 transform);
	PHYSICS_API RigidBodyHandle Physics_CreateStaticConvexHull(PhysicsWorldHandle world, const float* points, int numPoints);
	PHYSICS_API void Physics_RemoveRigidBody(PhysicsWorldHandle world, RigidBodyHandle body);

	PHYSICS_API void Physics_GetRigidBodyTransform(RigidBodyHandle body, float* transformMatrix);
	PHYSICS_API void Physics_GetPosition(RigidBodyHandle body, Vec3* position);
	PHYSICS_API void Physics_SetWorldTransform(RigidBodyHandle body, Mat4 transform);
	PHYSICS_API void Physics_SetLinearVelocity(RigidBodyHandle body, Vec3 velocity);
	PHYSICS_API void Physics_ApplyCentralImpulse(RigidBodyHandle body, Vec3 impulse);
	PHYSICS_API void Physics_Activate(RigidBodyHandle body);
	PHYSICS_API Vec3 Physics_GetLinearVelocity(RigidBodyHandle body);
	PHYSICS_API void Physics_SetGravityEnabled(RigidBodyHandle body, bool enabled);
	PHYSICS_API void Physics_ToggleCollision(PhysicsWorldHandle world, RigidBodyHandle body, bool enabled);
	PHYSICS_API void Physics_Teleport(RigidBodyHandle body, Vec3 position);
	PHYSICS_API void Physics_RecheckCollision(PhysicsWorldHandle world, RigidBodyHandle body);

	PHYSICS_API bool Physics_Raycast(PhysicsWorldHandle world, Vec3 start, Vec3 end, RaycastHitInfo* hitInfo);
	PHYSICS_API void Physics_ApplyImpulse(RigidBodyHandle bodyHandle, Vec3 impulse, Vec3 rel_pos);
	PHYSICS_API void Physics_ApplyBuoyancyInVolume(PhysicsWorldHandle handle, const float* vertices, int numVertices, const Mat4* transform);
	PHYSICS_API void Physics_SetDeactivationEnabled(PhysicsWorldHandle handle, bool enabled);

#ifdef __cplusplus
}
#endif

#endif // PHYSICS_WRAPPER_H