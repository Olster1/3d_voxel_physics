struct CollisionPoint {
    //NOTE: Data
    float3 point;
    float3 normal;
    float velocityBias; //NOTE: This is an added velocity to stop penetration along the collision normal
    float seperation; //NOTE: Negative if shapes are colliding. 
    float3 seperationVector; 
    float Pn; //NOTE: Accumulated normal impulse

    //NOTE: Used for the id
    EntityID entityId;
    int x;
    int y;
    int z;

    int x1;
    int y1;
    int z1;
};

float12 modelIntertiaTensorToWorld(float12 I, float4 q) {
    float12 rot = quaternionToMatrix3x3(q);
    float12 rotTransposed = float12_transpose(rot);

    float12 result = float12_multiply(float12_multiply(rot, I), rotTransposed);
    return result;
}

float3 voxelToWorldP(VoxelEntity *e, int x, int y, int z);

struct Arbiter {
    VoxelEntity *a;
    VoxelEntity *b;
    int pointsCount;
    CollisionPoint points[MAX_CONTACT_POINTS_PER_PAIR];

    Arbiter *next;
};

struct PhysicsWorld {
    Arbiter *arbiters;
    Arbiter *arbitersFreeList;

    bool positionCorrecting; //DONE: this is how agressively the physics tries and resolves penetration

    //NOTE: Erin Catto's talk at GDC 2006
};

float2 worldPToVoxelP(VoxelEntity *e, float2 worldP);

float calculateInverseMassNormal(CollisionPoint *p, VoxelEntity *a, VoxelEntity *b) {

    float12 inverseI_aWorld = modelIntertiaTensorToWorld(a->invI, a->T.rotation);
    float12 inverseI_bWorld = modelIntertiaTensorToWorld(b->invI, b->T.rotation);

    float3 relA = minus_float3(p->point, a->T.pos);
	float3 relB = minus_float3(p->point, b->T.pos);

    float kNormal = (a->inverseMass + b->inverseMass);
    kNormal += float3_dot(p->normal, float12_scale(inverseI_aWorld, float3_cross(float3_cross(relA, p->normal), relA)));
    kNormal += float3_dot(p->normal, float12_scale(inverseI_bWorld, float3_cross(float3_cross(relB, p->normal), relB)));
    
    if(kNormal == 0) {
        kNormal = 1;
    }
    return 1.0f / kNormal;

}

void prestepAllArbiters(PhysicsWorld *world, float inverseDt) {
    Arbiter *arb = world->arbiters;

    float allowedPenertration = 0.01;//NOTE: Meters

    //NOTE: If positionCorrection correction is on
    // The bias factor is important because overly aggressive corrections (with a high bias factor) can cause instability or jittering in the simulation, while too small of a correction (with a low bias factor) may leave objects slightly penetrated.
	// It strikes a balance between stability and realism, ensuring that objects resolve overlaps without visibly popping or jittering in the simulation.
    float biasFactor = (world->positionCorrecting) ? 0.1f : 0.0f;

    while(arb) {
        VoxelEntity *a = arb->a;
        VoxelEntity *b = arb->b;

        for(int i = 0; i < arb->pointsCount; i++) {
            CollisionPoint *p = &arb->points[i];
            
            //NOTE: The reason Baumgarte Stabilization is wrong is that it adds kinetic energy into the system 
            //      so energy is not conserved. Instead we have a seperate impulse(momentum) that tries and seperates the 
            //      position overlap. This way the velocities aren't contributing to contact normals. 
            //      To do this we get rid of the velocityBias contributing to the velocity constraint solver and have a seperate
            //      position constraint solver that tries and fixes the positions. 

            // PGS_NGS: This is the PGS solver with a NGS position solver instead of Baumgarte Stabilization. NGS was a nice feature in Box2D because it meant that objects that start with overlap do not fly apart, instead they are pushed apart gently.
            //NOTE: This is the Baumgarte Stabilization. This was upgraded to Nonlinear Gauss-Seidel (NGS) on box2d 1.0
            p->velocityBias = -biasFactor * MathMinf((p->seperation + allowedPenertration), 0) * inverseDt;

            float3 r1 = minus_float3(p->point, a->T.pos);
		    float3 r2 = minus_float3(p->point, b->T.pos);

            //NOTE: The accumulated impulse here is a convergence of the 'true' impulse 
            // Apply normal + friction impulse
            float3 Pn = scale_float3(p->Pn, p->normal);

            a->dP = minus_float3(a->dP, scale_float3(a->inverseMass, Pn));
            b->dP = plus_float3(b->dP, scale_float3(b->inverseMass, Pn));

            a->dA = minus_float3(a->dA, float12_scale(a->invI, float3_cross(r1, Pn)));
            b->dA = plus_float3(b->dA, float12_scale(b->invI, float3_cross(r2, Pn)));
        }

        arb = arb->next;
    }
}

void runPGSSolver(PhysicsWorld *world) {
    const int iterationCount = PHYSICS_ITERATION_COUNT;
    
    Arbiter *arb = world->arbiters;
    
    for(int m = 0; m < iterationCount; m++) {
        while(arb) {
            VoxelEntity *a = arb->a;
            VoxelEntity *b = arb->b;
            for(int i = 0; i < arb->pointsCount; i++) {
                global_updateArbCount = arb->pointsCount;
                CollisionPoint *p = &arb->points[i];

                float e = MathMaxf(a->coefficientOfRestitution, b->coefficientOfRestitution);

                const float3 velocityA = a->dP;
                const float3 velocityB = b->dP;

                //NOTE: Get the distance of the point from center of mass 
                float3 r1 = minus_float3(p->point, a->T.pos);
                float3 r2 = minus_float3(p->point, b->T.pos);

                float3 dpPointA = plus_float3(velocityA, float3_cross(a->dA, r1));
                float3 dpPointB = plus_float3(velocityB, float3_cross(b->dA, r2));

                // Relative velocity at contact
                float3 relativeAB = minus_float3(dpPointB, dpPointA);

                if(float3_dot(relativeAB, relativeAB) < (PHYSICS_RESTITUTION_VELOCITY_THRESHOLD_SQR)) 
                {
                    e = 0;
                }

                float vn = float3_dot(relativeAB, p->normal);

                //NOTE: This is the J calculation as in Chris Hecker's phsyics tutorials. Which is actually the conservation of momentum and Newton's third law - equal and opposite reaction
                float dPn = ((-(1 + e)*vn) + p->velocityBias) * calculateInverseMassNormal(p, a, b);

                {
                    //NOTE: accumulateImpulses
                    assert(p->Pn >= 0);
                    float Pn0 = p->Pn; //NOTE: Impulse magnitude previous frame
                    p->Pn = MathMaxf(Pn0 + dPn, 0.0f);
                    dPn = p->Pn - Pn0;
                }

                float3 Pn = scale_float3(dPn, p->normal);

                a->dP = minus_float3(arb->a->dP, scale_float3(a->inverseMass, Pn));
                b->dP = plus_float3(arb->b->dP, scale_float3(b->inverseMass, Pn));

                a->dA = minus_float3(a->dA, float12_scale(a->invI, float3_cross(r1, Pn)));
                b->dA = plus_float3(b->dA, float12_scale(b->invI, float3_cross(r2, Pn)));
            }
            arb = arb->next;
        }
    }

}

void wakeUpEntity(VoxelEntity *e) {
    e->asleep = false;
    e->sleepTimer = 0;
} 

void mergePointsToArbiter(PhysicsWorld *world, CollisionPoint *points, int pointCount, VoxelEntity *a, VoxelEntity *b) {
    if(a > b) {
        VoxelEntity *temp = b;
        b = a;
        a = temp;
    }

    Arbiter *arb = world->arbiters;
    Arbiter **arbPrev = &world->arbiters;
    while(arb) {
        if(arb->a == a && arb->b == b) {
            if(pointCount == 0) {
                //NOTE: Remove from the list
                *arbPrev = arb->next;
                arb->next = world->arbitersFreeList;
                world->arbitersFreeList = arb;
            }
            break;
        } else {
            arbPrev = &arb->next;
            arb = arb->next;
        }
    }

    if(pointCount == 0) {
        
    } else {
        
        if(!arb) {
            if(world->arbitersFreeList) {
                arb = world->arbitersFreeList;
                world->arbitersFreeList = arb->next;
            } else {
                arb = pushStruct(&globalLongTermArena, Arbiter);
            }
            arb->next = world->arbiters;
            world->arbiters = arb;

            arb->pointsCount = 0;

            arb->a = a;
            arb->b = b;
        }

        assert(arb);
        assert(arb->a);
        assert(arb->b);

        
        for(int j = 0; j < pointCount; j++) {
            CollisionPoint *newP = &points[j];
            bool pointFound = false;
            for(int i = 0; i < arb->pointsCount && !pointFound; i++) {
                CollisionPoint *oldP = &arb->points[i];
            
                if(oldP->x == newP->x && oldP->y == newP->y && oldP->z == newP->z && areEntityIdsEqual(oldP->entityId, newP->entityId)) {
                    //NOTE: Point already exists, so keep the accumulated impluses going
                    newP->Pn = oldP->Pn;
                    // c->Pt = cOld->Pt;
                    // c->Pnb = cOld->Pnb;
                    pointFound = true;
                }
            }
        }

        assert(pointCount <= arrayCount(arb->points));
        arb->pointsCount = pointCount;

        //NOTE: Copy the new points array to the old points array
        for(int i = 0; i < pointCount; i++) {
            arb->points[i] = points[i];
        }
    }
}