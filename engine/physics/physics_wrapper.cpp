/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
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
#include <btBulletDynamicsCommon.h>
#include <vector>
#include <map>

#include "physics_wrapper.h"

static bool g_physics_deactivation_enabled = true;

struct PhysicsWorld {
    btDiscreteDynamicsWorld* dynamicsWorld;
    btDefaultCollisionConfiguration* collisionConfiguration;
    btCollisionDispatcher* dispatcher;
    btBroadphaseInterface* overlappingPairCache;
    btSequentialImpulseConstraintSolver* solver;
    std::vector<btCollisionShape*> collisionShapes;
    std::map<btRigidBody*, btTriangleIndexVertexArray*> meshInterfaces;
};

struct RigidBody {
    btRigidBody* body;
};

extern "C" {

    PhysicsWorldHandle Physics_CreateWorld(float gravity_y) {
        PhysicsWorld* world = new PhysicsWorld();
        world->collisionConfiguration = new btDefaultCollisionConfiguration();
        world->dispatcher = new btCollisionDispatcher(world->collisionConfiguration);
        world->overlappingPairCache = new btDbvtBroadphase();
        world->solver = new btSequentialImpulseConstraintSolver();
        world->dynamicsWorld = new btDiscreteDynamicsWorld(world->dispatcher, world->overlappingPairCache, world->solver, world->collisionConfiguration);
        world->dynamicsWorld->setGravity(btVector3(0, gravity_y, 0));
        return world;
    }

    void Physics_DestroyWorld(PhysicsWorldHandle handle) {
        if (!handle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        for (int i = world->dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = world->dynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMotionState()) {
                delete body->getMotionState();
            }
            world->dynamicsWorld->removeCollisionObject(obj);
            if (body && world->meshInterfaces.count(body)) {
                delete world->meshInterfaces[body];
                world->meshInterfaces.erase(body);
            }
            delete obj;
        }

        for (size_t i = 0; i < world->collisionShapes.size(); i++) {
            delete world->collisionShapes[i];
        }
        world->collisionShapes.clear();
        world->meshInterfaces.clear();

        delete world->dynamicsWorld;
        delete world->solver;
        delete world->overlappingPairCache;
        delete world->dispatcher;
        delete world->collisionConfiguration;
        delete world;
    }

    void Physics_StepSimulation(PhysicsWorldHandle handle, float deltaTime) {
        if (!handle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        world->dynamicsWorld->stepSimulation(deltaTime, 10, 1.0f / 60.0f);
    }

    RigidBodyHandle Physics_CreatePlayerCapsule(PhysicsWorldHandle handle, float radius, float totalHeight, float mass, Vec3 startPos) {
        if (!handle) return NULL;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        float cylinderHeight = totalHeight - (2.0f * radius);
        if (cylinderHeight < 0) {
            cylinderHeight = 0;
        }

        btCollisionShape* capsuleShape = new btCapsuleShape(radius, cylinderHeight);
        world->collisionShapes.push_back(capsuleShape);

        btTransform startTransform;
        startTransform.setIdentity();

        btVector3 capsuleCenter(startPos.x, startPos.y + totalHeight / 2.0f, startPos.z);
        startTransform.setOrigin(capsuleCenter);

        btVector3 localInertia(0, 0, 0);
        if (mass != 0.0f) {
            capsuleShape->calculateLocalInertia(mass, localInertia);
        }

        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, capsuleShape, localInertia);
        btRigidBody* body = new btRigidBody(rbInfo);

        body->setAngularFactor(btVector3(0, 1, 0));
        if (!g_physics_deactivation_enabled) {
            body->setActivationState(DISABLE_DEACTIVATION);
        }
        body->setFriction(0.2f);

        world->dynamicsWorld->addRigidBody(body, COL_PLAYER, COL_STATIC | COL_DYNAMIC);

        RigidBody* rb = new RigidBody();
        rb->body = body;
        return rb;
    }

    RigidBodyHandle Physics_CreateDynamicConvexHull(PhysicsWorldHandle handle, const float* points, int numPoints, float mass, Mat4 transform) {
        if (!handle || !points || numPoints == 0) return NULL;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        btConvexHullShape* hullShape = new btConvexHullShape(points, numPoints, 3 * sizeof(float));
        world->collisionShapes.push_back(hullShape);

        btTransform startTransform;
        startTransform.setFromOpenGLMatrix(transform.m);

        btVector3 localInertia(0, 0, 0);
        if (mass != 0.0f) {
            hullShape->calculateLocalInertia(mass, localInertia);
        }

        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, hullShape, localInertia);
        btRigidBody* body = new btRigidBody(rbInfo);

        body->setFriction(0.8f);
        body->setRestitution(0.2f);
        body->setDamping(0.2f, 0.5f);

        if (mass > 0.0f) {
            world->dynamicsWorld->addRigidBody(body, COL_DYNAMIC, COL_ALL);
        }

        RigidBody* rb = new RigidBody();
        rb->body = body;
        return rb;
    }

    RigidBodyHandle Physics_CreateDynamicBrush(PhysicsWorldHandle handle, const float* vertices, int numVertices, float mass, Mat4 transform) {
        if (!handle || !vertices || numVertices == 0 || mass <= 0.0f) return NULL;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        btConvexHullShape* hullShape = new btConvexHullShape(vertices, numVertices, 3 * sizeof(float));
        world->collisionShapes.push_back(hullShape);

        btTransform startTransform;
        startTransform.setFromOpenGLMatrix(transform.m);

        btVector3 localInertia(0, 0, 0);
        hullShape->calculateLocalInertia(mass, localInertia);

        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, hullShape, localInertia);
        btRigidBody* body = new btRigidBody(rbInfo);

        body->setFriction(0.7f);
        body->setRestitution(0.1f);
        body->setDamping(0.2f, 0.5f);

        world->dynamicsWorld->addRigidBody(body, COL_DYNAMIC, COL_ALL);

        RigidBody* rb = new RigidBody();
        rb->body = body;
        return rb;
    }

    RigidBodyHandle Physics_CreateStaticTriangleMesh(PhysicsWorldHandle handle, const float* vertices, int numVertices, const unsigned int* indices, int numIndices, Mat4 transform, Vec3 scale) {
        if (!handle || !vertices || numVertices == 0 || !indices || numIndices == 0) return NULL;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        btIndexedMesh mesh;
        mesh.m_numTriangles = numIndices / 3;
        mesh.m_triangleIndexBase = (const unsigned char*)indices;
        mesh.m_triangleIndexStride = 3 * sizeof(unsigned int);
        mesh.m_indexType = PHY_INTEGER;

        mesh.m_numVertices = numVertices;
        mesh.m_vertexBase = (const unsigned char*)vertices;
        mesh.m_vertexStride = 3 * sizeof(float);
        mesh.m_vertexType = PHY_FLOAT;

        btTriangleIndexVertexArray* meshInterface = new btTriangleIndexVertexArray();
        meshInterface->addIndexedMesh(mesh, PHY_INTEGER);

        btBvhTriangleMeshShape* meshShape = new btBvhTriangleMeshShape(meshInterface, false, true);

        meshShape->setLocalScaling(btVector3(scale.x, scale.y, scale.z));

        world->collisionShapes.push_back(meshShape);

        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setFromOpenGLMatrix(transform.m);

        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, myMotionState, meshShape, btVector3(0, 0, 0));
        btRigidBody* body = new btRigidBody(rbInfo);

        body->setFriction(1.0f);
        if (!g_physics_deactivation_enabled) {
            body->setActivationState(DISABLE_DEACTIVATION);
        }
        world->dynamicsWorld->addRigidBody(body, COL_STATIC, COL_PLAYER | COL_DYNAMIC);
        world->meshInterfaces[body] = meshInterface;

        RigidBody* rb = new RigidBody();
        rb->body = body;
        return rb;
    }

    RigidBodyHandle Physics_CreateStaticConvexHull(PhysicsWorldHandle handle, const float* points, int numPoints) {
        if (!handle || !points || numPoints == 0) return NULL;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        btConvexHullShape* hullShape = new btConvexHullShape(points, numPoints, 3 * sizeof(float));

        world->collisionShapes.push_back(hullShape);

        btTransform startTransform;
        startTransform.setIdentity();

        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, myMotionState, hullShape, btVector3(0, 0, 0));
        btRigidBody* body = new btRigidBody(rbInfo);

        body->setFriction(1.0f);
        world->dynamicsWorld->addRigidBody(body, COL_STATIC, COL_PLAYER | COL_DYNAMIC);

        RigidBody* rb = new RigidBody();
        rb->body = body;
        return rb;
    }

    RigidBodyHandle Physics_CreateKinematicBrush(PhysicsWorldHandle handle, const float* vertices, int numVertices, Mat4 transform) {
        if (!handle || !vertices || numVertices == 0) return NULL;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        btConvexHullShape* hullShape = new btConvexHullShape(vertices, numVertices, 3 * sizeof(float));
        world->collisionShapes.push_back(hullShape);

        btTransform startTransform;
        startTransform.setFromOpenGLMatrix(transform.m);

        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, myMotionState, hullShape, btVector3(0, 0, 0));
        btRigidBody* body = new btRigidBody(rbInfo);

        body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        body->setActivationState(DISABLE_DEACTIVATION);

        world->dynamicsWorld->addRigidBody(body, COL_STATIC, COL_PLAYER | COL_DYNAMIC);

        RigidBody* rb = new RigidBody();
        rb->body = body;
        return rb;
    }

    void Physics_RemoveRigidBody(PhysicsWorldHandle handle, RigidBodyHandle bodyHandle) {
        if (!handle || !bodyHandle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        RigidBody* rb = (RigidBody*)bodyHandle;

        if (!rb->body) {
            delete rb;
            return;
        }

        if (rb->body) {
            btCollisionShape* shape = rb->body->getCollisionShape();
            if (rb->body->getMotionState()) {
                delete rb->body->getMotionState();
            }
            world->dynamicsWorld->removeRigidBody(rb->body);
            if (world->meshInterfaces.count(rb->body)) {
                delete world->meshInterfaces[rb->body];
                world->meshInterfaces.erase(rb->body);
            }
            delete rb->body;

            for (auto it = world->collisionShapes.begin(); it != world->collisionShapes.end(); ++it) {
                if (*it == shape) {
                    world->collisionShapes.erase(it);
                    delete shape;
                    break;
                }
            }
        }
        delete rb;
    }

    void Physics_GetRigidBodyTransform(RigidBodyHandle bodyHandle, float* transformMatrix) {
        if (!bodyHandle || !transformMatrix) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        btTransform trans;
        if (rb->body && rb->body->getMotionState()) {
            rb->body->getMotionState()->getWorldTransform(trans);
        }
        else {
            trans = rb->body->getWorldTransform();
        }
        trans.getOpenGLMatrix(transformMatrix);
    }

    void Physics_GetPosition(RigidBodyHandle bodyHandle, Vec3* position) {
        if (!bodyHandle || !position) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        const btVector3& pos = rb->body->getWorldTransform().getOrigin();
        position->x = pos.x();
        position->y = pos.y();
        position->z = pos.z();
    }

    void Physics_SetWorldTransform(RigidBodyHandle bodyHandle, Mat4 transform) {
        if (!bodyHandle) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        if (rb->body) {
            btTransform trans;
            trans.setFromOpenGLMatrix(transform.m);
            if (rb->body->getMotionState()) {
                rb->body->getMotionState()->setWorldTransform(trans);
            }
            rb->body->setWorldTransform(trans);
            rb->body->activate(true);
        }
    }

    void Physics_SetLinearVelocity(RigidBodyHandle bodyHandle, Vec3 velocity) {
        if (!bodyHandle) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        rb->body->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    }

    void Physics_ApplyCentralImpulse(RigidBodyHandle bodyHandle, Vec3 impulse) {
        if (!bodyHandle) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        rb->body->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
    }

    void Physics_Activate(RigidBodyHandle bodyHandle) {
        if (!bodyHandle) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        rb->body->activate(true);
    }

    Vec3 Physics_GetLinearVelocity(RigidBodyHandle bodyHandle) {
        if (!bodyHandle) return Vec3{ 0,0,0 };
        RigidBody* rb = (RigidBody*)bodyHandle;
        btVector3 vel = rb->body->getLinearVelocity();
        return Vec3{ vel.x(), vel.y(), vel.z() };
    }

    void Physics_SetGravityEnabled(RigidBodyHandle bodyHandle, bool enabled) {
        if (!bodyHandle) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        int flags = rb->body->getFlags();
        if (enabled) {
            rb->body->setFlags(flags & ~BT_DISABLE_WORLD_GRAVITY);
        }
        else {
            rb->body->setFlags(flags | BT_DISABLE_WORLD_GRAVITY);
        }
        rb->body->activate(true);
    }

    void Physics_ToggleCollision(PhysicsWorldHandle handle, RigidBodyHandle bodyHandle, bool enabled) {
        if (!handle || !bodyHandle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        RigidBody* rb = (RigidBody*)bodyHandle;

        if (!rb || !rb->body) return;

        bool isInWorld = false;
        for (int i = 0; i < world->dynamicsWorld->getNumCollisionObjects(); ++i) {
            if (world->dynamicsWorld->getCollisionObjectArray()[i] == rb->body) {
                isInWorld = true;
                break;
            }
        }

        if (enabled && !isInWorld) {
            world->dynamicsWorld->addRigidBody(rb->body);
            rb->body->activate(true);
        }
        else if (!enabled && isInWorld) {
            world->dynamicsWorld->removeRigidBody(rb->body);
        }
    }

    void Physics_Teleport(RigidBodyHandle bodyHandle, Vec3 position) {
        if (!bodyHandle) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        btTransform trans = rb->body->getWorldTransform();
        trans.setOrigin(btVector3(position.x, position.y, position.z));
        rb->body->setWorldTransform(trans);
        rb->body->setLinearVelocity(btVector3(0, 0, 0));
        rb->body->setAngularVelocity(btVector3(0, 0, 0));
        rb->body->clearForces();
        rb->body->activate(true);
    }

    void Physics_RecheckCollision(PhysicsWorldHandle handle, RigidBodyHandle bodyHandle) {
        if (!handle || !bodyHandle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        RigidBody* rb = (RigidBody*)bodyHandle;

        if (world->dynamicsWorld && rb->body) {
            world->dynamicsWorld->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(rb->body->getBroadphaseHandle(), world->dynamicsWorld->getDispatcher());
        }
    }

    bool Physics_Raycast(PhysicsWorldHandle handle, Vec3 start, Vec3 end, RaycastHitInfo* hitInfo) {
        if (!handle || !hitInfo) return false;
        PhysicsWorld* world = (PhysicsWorld*)handle;

        btVector3 btStart(start.x, start.y, start.z);
        btVector3 btEnd(end.x, end.y, end.z);

        btCollisionWorld::ClosestRayResultCallback rayCallback(btStart, btEnd);

        rayCallback.m_collisionFilterGroup = COL_ALL;
        rayCallback.m_collisionFilterMask = COL_ALL & ~COL_PLAYER;

        world->dynamicsWorld->rayTest(btStart, btEnd, rayCallback);

        if (rayCallback.hasHit()) {
            hitInfo->hasHit = true;
            const btVector3& point = rayCallback.m_hitPointWorld;
            const btVector3& normal = rayCallback.m_hitNormalWorld;
            hitInfo->point = { point.x(), point.y(), point.z() };
            hitInfo->normal = { normal.x(), normal.y(), normal.z() };

            RigidBody* rb = new RigidBody();
            rb->body = (btRigidBody*)btRigidBody::upcast(rayCallback.m_collisionObject);
            hitInfo->hitBody = rb;

            return true;
        }

        hitInfo->hasHit = false;
        return false;
    }

    void Physics_ApplyImpulse(RigidBodyHandle bodyHandle, Vec3 impulse, Vec3 rel_pos) {
        if (!bodyHandle) return;
        RigidBody* rb = (RigidBody*)bodyHandle;
        if (rb->body && rb->body->getMass() > 0.0f) {
            rb->body->activate(true);
            rb->body->applyImpulse(btVector3(impulse.x, impulse.y, impulse.z), btVector3(rel_pos.x, rel_pos.y, rel_pos.z));
        }
    }

    void Physics_ApplyBuoyancyInVolume(PhysicsWorldHandle handle, const float* vertices, int numVertices, const Mat4* transform) {
        if (!handle || !vertices || numVertices == 0) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        btDynamicsWorld* dynamicsWorld = world->dynamicsWorld;
        btVector3 gravity = dynamicsWorld->getGravity();

        btTransform brushTransform;
        brushTransform.setFromOpenGLMatrix(transform->m);

        btVector3 brush_min(FLT_MAX, FLT_MAX, FLT_MAX);
        btVector3 brush_max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (int v_idx = 0; v_idx < numVertices; ++v_idx) {
            btVector3 local_v(vertices[v_idx * 7 + 0], vertices[v_idx * 7 + 1], vertices[v_idx * 7 + 2]);
            btVector3 world_v = brushTransform * local_v;
            brush_min.setMin(world_v);
            brush_max.setMax(world_v);
        }

        for (int i = 0; i < dynamicsWorld->getNumCollisionObjects(); ++i) {
            btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);

            if (body && body->getMass() > 0.0f) {
                btVector3 bodyPos = body->getWorldTransform().getOrigin();

                if (bodyPos.x() > brush_min.x() && bodyPos.x() < brush_max.x() &&
                    bodyPos.y() > brush_min.y() && bodyPos.y() < brush_max.y() &&
                    bodyPos.z() > brush_min.z() && bodyPos.z() < brush_max.z())
                {
                    float buoyancyMultiplier = 1.5f;
                    btVector3 buoyancyForce = -gravity * body->getMass() * buoyancyMultiplier;
                    body->applyCentralForce(buoyancyForce);

                    btVector3 velocity = body->getLinearVelocity();
                    btVector3 dragForce = -velocity * 0.9f;
                    body->applyCentralForce(dragForce);

                    btVector3 angularVelocity = body->getAngularVelocity();
                    btVector3 angularDrag = -angularVelocity * 0.5f;
                    body->applyTorque(angularDrag);
                }
            }
        }
    }

    void Physics_SetDeactivationEnabled(PhysicsWorldHandle handle, bool enabled) {
        if (!handle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        g_physics_deactivation_enabled = enabled;

        for (int i = 0; i < world->dynamicsWorld->getNumCollisionObjects(); ++i) {
            btCollisionObject* obj = world->dynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);

            if (body && body->getInvMass() > 0.0f) {
                if (enabled) {
                    body->forceActivationState(ACTIVE_TAG);
                    body->activate(true);
                }
                else {
                    body->setActivationState(DISABLE_DEACTIVATION);
                }
            }
        }
    }

    PHYSICS_API bool Physics_CheckGroundContact(PhysicsWorldHandle handle, RigidBodyHandle bodyHandle, float groundCheckDistance) {
        if (!handle || !bodyHandle) return false;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        RigidBody* rb = (RigidBody*)bodyHandle;

        if (!rb->body || rb->body->getCollisionShape()->getShapeType() != CAPSULE_SHAPE_PROXYTYPE) {
            return false;
        }

        btTransform trans;
        rb->body->getMotionState()->getWorldTransform(trans);
        btVector3 startPos = trans.getOrigin();

        btCapsuleShape* shape = static_cast<btCapsuleShape*>(rb->body->getCollisionShape());
        float halfHeight = shape->getHalfHeight();
        float radius = shape->getRadius();

        btVector3 rayStart = startPos;
        btVector3 rayEnd = startPos - btVector3(0, halfHeight + radius + groundCheckDistance, 0);

        btCollisionWorld::ClosestRayResultCallback rayCallback(rayStart, rayEnd);
        rayCallback.m_collisionFilterGroup = COL_PLAYER;
        rayCallback.m_collisionFilterMask = COL_STATIC | COL_DYNAMIC;

        world->dynamicsWorld->rayTest(rayStart, rayEnd, rayCallback);

        if (rayCallback.hasHit()) {
            btVector3 upVector(0.0f, 1.0f, 0.0f);
            if (rayCallback.m_hitNormalWorld.dot(upVector) > 0.7f) {
                return true;
            }
        }
        return false;
    }

    void Physics_SetGravity(PhysicsWorldHandle handle, Vec3 gravity) {
        if (!handle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        world->dynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));

        for (int i = 0; i < world->dynamicsWorld->getNumCollisionObjects(); ++i) {
            btCollisionObject* obj = world->dynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getInvMass() > 0.0f) {
                body->activate(true);
            }
        }
    }
}