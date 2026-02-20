/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once
#ifndef PHYSICS_WRAPPER_H
#define PHYSICS_WRAPPER_H

//----------------------------------------//
// Brief: Physics wrapper written in C++ using bullet
//----------------------------------------//

#include "math_lib.h"
#include "physics_api.h"


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
		Bool hasHit;
		Vec3 point;
		Vec3 normal;
		RigidBodyHandle hitBody;
	} RaycastHitInfo;

	namespace Physics {
		PHYSICS_API PhysicsWorldHandle CreateWorld(Float gravity_y);
		PHYSICS_API void DestroyWorld(PhysicsWorldHandle world);
		PHYSICS_API void StepSimulation(PhysicsWorldHandle world, Float deltaTime);

		PHYSICS_API RigidBodyHandle CreatePlayerCapsule(PhysicsWorldHandle world, Float radius, Float height, Float mass, Vec3 startPos);
		PHYSICS_API RigidBodyHandle CreateStaticTriangleMesh(PhysicsWorldHandle world, const Float* vertices, Int numVertices, const Uint* indices, Int numIndices, Mat4 transform, Vec3 scale);
		PHYSICS_API RigidBodyHandle CreateDynamicConvexHull(PhysicsWorldHandle world, const Float* points, Int numPoints, Float mass, Mat4 transform);
		PHYSICS_API RigidBodyHandle CreateDynamicBrush(PhysicsWorldHandle world, const Float* vertices, Int numVertices, Int stride, Float mass, Mat4 transform);
		PHYSICS_API RigidBodyHandle CreateStaticConvexHull(PhysicsWorldHandle world, const Float* points, Int numPoints);
		PHYSICS_API RigidBodyHandle CreateKinematicBrush(PhysicsWorldHandle world, const Float* vertices, Int numVertices, Mat4 transform);
		PHYSICS_API void RemoveRigidBody(PhysicsWorldHandle world, RigidBodyHandle body);

		PHYSICS_API void GetRigidBodyTransform(RigidBodyHandle body, Float* transformMatrix);
		PHYSICS_API void GetPosition(RigidBodyHandle body, Vec3* position);
		PHYSICS_API void SetWorldTransform(RigidBodyHandle body, Mat4 transform);
		PHYSICS_API void SetLinearVelocity(RigidBodyHandle body, Vec3 velocity);
		PHYSICS_API void ApplyCentralImpulse(RigidBodyHandle body, Vec3 impulse);
		PHYSICS_API void Activate(RigidBodyHandle body);
		PHYSICS_API Vec3 GetLinearVelocity(RigidBodyHandle body);
		PHYSICS_API void SetGravityEnabled(RigidBodyHandle body, Bool enabled);
		PHYSICS_API void ToggleCollision(PhysicsWorldHandle world, RigidBodyHandle body, Bool enabled);
		PHYSICS_API void Teleport(RigidBodyHandle body, Vec3 position);
		PHYSICS_API void RecheckCollision(PhysicsWorldHandle world, RigidBodyHandle body);

		PHYSICS_API Bool Raycast(PhysicsWorldHandle world, Vec3 start, Vec3 end, RaycastHitInfo* hitInfo);
		PHYSICS_API Float GetMass(RigidBodyHandle bodyHandle);
		PHYSICS_API void SetCcdEnabled(RigidBodyHandle bodyHandle, Bool enabled, Float motion_threshold);
		PHYSICS_API void ApplyImpulse(RigidBodyHandle bodyHandle, Vec3 impulse, Vec3 rel_pos);
		PHYSICS_API void ApplyBuoyancyInVolume(PhysicsWorldHandle handle, const Float* vertices, Int numVertices, const Mat4* transform);
		PHYSICS_API void SetDeactivationEnabled(PhysicsWorldHandle handle, Bool enabled);
		PHYSICS_API Bool CheckGroundContact(PhysicsWorldHandle handle, RigidBodyHandle bodyHandle, Float groundCheckDistance);
		PHYSICS_API Float GetTotalMassOnObject(PhysicsWorldHandle handle, RigidBodyHandle bodyHandle);
		PHYSICS_API void SetGravity(PhysicsWorldHandle handle, Vec3 gravity);
	}


#endif // PHYSICS_WRAPPER_H