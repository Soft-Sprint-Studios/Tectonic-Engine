/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include <GL/glew.h>
#include <btBulletDynamicsCommon.h>
#include <vector>
#include <map>

extern "C" {
#include "physics_wrapper.h"
}

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
        body->setActivationState(DISABLE_DEACTIVATION);
        body->setFriction(0.7f);

        world->dynamicsWorld->addRigidBody(body);

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

        if (mass > 0.0f) {
            world->dynamicsWorld->addRigidBody(body);
        }

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
        body->setActivationState(DISABLE_DEACTIVATION);
        world->dynamicsWorld->addRigidBody(body);
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
        world->dynamicsWorld->addRigidBody(body);

        RigidBody* rb = new RigidBody();
        rb->body = body;
        return rb;
    }

    void Physics_RemoveRigidBody(PhysicsWorldHandle handle, RigidBodyHandle bodyHandle) {
        if (!handle || !bodyHandle) return;
        PhysicsWorld* world = (PhysicsWorld*)handle;
        RigidBody* rb = (RigidBody*)bodyHandle;

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
}