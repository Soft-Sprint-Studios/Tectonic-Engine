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

#include "math_lib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct PhysicsWorld* PhysicsWorldHandle;
	typedef struct RigidBody* RigidBodyHandle;

	typedef struct {
		bool hasHit;
		Vec3 point;
		Vec3 normal;
		RigidBodyHandle hitBody;
	} RaycastHitInfo;

	PhysicsWorldHandle Physics_CreateWorld(float gravity_y);
	void Physics_DestroyWorld(PhysicsWorldHandle world);
	void Physics_StepSimulation(PhysicsWorldHandle world, float deltaTime);

	RigidBodyHandle Physics_CreatePlayerCapsule(PhysicsWorldHandle world, float radius, float height, float mass, Vec3 startPos);
	RigidBodyHandle Physics_CreateStaticTriangleMesh(PhysicsWorldHandle world, const float* vertices, int numVertices, const unsigned int* indices, int numIndices, Mat4 transform, Vec3 scale);
	RigidBodyHandle Physics_CreateDynamicConvexHull(PhysicsWorldHandle world, const float* points, int numPoints, float mass, Mat4 transform);
	RigidBodyHandle Physics_CreateStaticConvexHull(PhysicsWorldHandle world, const float* points, int numPoints);
	void Physics_RemoveRigidBody(PhysicsWorldHandle world, RigidBodyHandle body);

	void Physics_GetRigidBodyTransform(RigidBodyHandle body, float* transformMatrix);
	void Physics_GetPosition(RigidBodyHandle body, Vec3* position);
	void Physics_SetWorldTransform(RigidBodyHandle body, Mat4 transform);
	void Physics_SetLinearVelocity(RigidBodyHandle body, Vec3 velocity);
	void Physics_ApplyCentralImpulse(RigidBodyHandle body, Vec3 impulse);
	void Physics_Activate(RigidBodyHandle body);
	Vec3 Physics_GetLinearVelocity(RigidBodyHandle body);
	void Physics_SetGravityEnabled(RigidBodyHandle body, bool enabled);
	void Physics_ToggleCollision(PhysicsWorldHandle world, RigidBodyHandle body, bool enabled);
	void Physics_Teleport(RigidBodyHandle body, Vec3 position);
	void Physics_RecheckCollision(PhysicsWorldHandle world, RigidBodyHandle body);

	bool Physics_Raycast(PhysicsWorldHandle world, Vec3 start, Vec3 end, RaycastHitInfo* hitInfo);
	void Physics_ApplyImpulse(RigidBodyHandle bodyHandle, Vec3 impulse, Vec3 rel_pos);

#ifdef __cplusplus
}
#endif

#endif // PHYSICS_WRAPPER_H