import { VehicleType, OrbType, PadType, SpecialBlockType, LevelObject, PlayerState } from '../types/gd';

export const PHYS_FPS = 240;
export const PHYS_DT = 1 / PHYS_FPS;

export const PHYS_SPEEDS = [
  251.16008,   // 0.5x
  311.579895,  // 1x
  387.420105,  // 2x
  468.0,       // 3x
  576.0002     // 4x
];

export const PLAYER_SIZE_NORMAL = { width: 30, height: 30 };
export const PLAYER_SIZE_MINI = { width: 18, height: 18 };

export class GDPhysicsEngine {
  public static createInitialState(vehicle: VehicleType = VehicleType.Cube, speed: number = 1): PlayerState {
    return {
      frame: 0,
      x: 0,
      y: 15,
      vx: PHYS_SPEEDS[speed] * PHYS_DT,
      vy: 0,
      rotation: 0,
      grounded: true,
      dead: false,
      upsideDown: false,
      small: false,
      speed,
      vehicle,
      isDashing: false,
      dashAngle: 0,
      hasHBlock: false,
      hasDBlock: false,
      hasJBlock: false,
      dual: false,
      mirror: false,
      input: false,
      coyoteFrames: 0,
      robotBoostFrames: 0,
    };
  }

  public static getPlayerSize(state: PlayerState): { width: number; height: number } {
    return state.small ? PLAYER_SIZE_MINI : PLAYER_SIZE_NORMAL;
  }

  public static getInnerHitbox(state: PlayerState) {
    const size = this.getPlayerSize(state);
    const innerW = size.width * 0.7;
    const innerH = size.height * 0.7;
    return {
      x: state.x - innerW / 2,
      y: state.y - innerH / 2,
      width: innerW,
      height: innerH,
    };
  }

  public static step(current: PlayerState, input: boolean, objects: LevelObject[]): PlayerState {
    if (current.dead) return { ...current, frame: current.frame + 1 };

    const next: PlayerState = {
      ...current,
      frame: current.frame + 1,
      input,
      hasHBlock: false,
      hasDBlock: false,
      hasJBlock: false,
      lastSpiderRay: undefined,
    };

    const size = this.getPlayerSize(next);
    const grav = next.upsideDown ? -1 : 1;
    const speedX = PHYS_SPEEDS[next.speed] * PHYS_DT;
    next.vx = speedX;

    // Handle Dash state
    if (next.isDashing) {
      if (!input) {
        next.isDashing = false;
      } else {
        next.vy = 0;
        next.grounded = false;
      }
    }

    // Nearby objects detection (spatial partition)
    const nearby = objects.filter(obj => 
      obj.x >= next.x - 120 && obj.x <= next.x + 120 &&
      obj.y >= next.y - 150 && obj.y <= next.y + 150
    );

    // 1. Process Special Blocks pre-check (S, J, H, D)
    for (const obj of nearby) {
      if (obj.category === 'special' && obj.specialType) {
        const pLeft = next.x - size.width / 2;
        const pRight = next.x + size.width / 2;
        const pBottom = next.y - size.height / 2;
        const pTop = next.y + size.height / 2;

        const oLeft = obj.x - obj.width / 2;
        const oRight = obj.x + obj.width / 2;
        const oBottom = obj.y - obj.height / 2;
        const oTop = obj.y + obj.height / 2;

        if (pRight > oLeft && pLeft < oRight && pTop > oBottom && pBottom < oTop) {
          if (obj.specialType === SpecialBlockType.S) next.isDashing = false;
          if (obj.specialType === SpecialBlockType.J) next.hasJBlock = true;
          if (obj.specialType === SpecialBlockType.H) next.hasHBlock = true;
          if (obj.specialType === SpecialBlockType.D) next.hasDBlock = true;
        }
      }
    }

    // 2. Process Portals, Pads & Orbs overlap
    const effectiveInput = next.hasJBlock ? false : input;
    const justPressed = effectiveInput && !current.input;

    for (const obj of nearby) {
      const pLeft = next.x - size.width / 2;
      const pRight = next.x + size.width / 2;
      const pBottom = next.y - size.height / 2;
      const pTop = next.y + size.height / 2;

      const oLeft = obj.x - obj.width / 2;
      const oRight = obj.x + obj.width / 2;
      const oBottom = obj.y - obj.height / 2;
      const oTop = obj.y + obj.height / 2;

      const isInside = pRight >= oLeft && pLeft <= oRight && pTop >= oBottom && pBottom <= oTop;

      if (isInside) {
        if (obj.category === 'portal') {
          if (obj.portalType === 'gravity_down') next.upsideDown = false;
          else if (obj.portalType === 'gravity_up') next.upsideDown = true;
          else if (obj.portalType === 'size_normal') next.small = false;
          else if (obj.portalType === 'size_mini') next.small = true;
          else if (obj.portalType === 'speed_05') next.speed = 0;
          else if (obj.portalType === 'speed_1') next.speed = 1;
          else if (obj.portalType === 'speed_2') next.speed = 2;
          else if (obj.portalType === 'speed_3') next.speed = 3;
          else if (obj.portalType === 'speed_4') next.speed = 4;
          else if (obj.portalType === 'mirror_on') next.mirror = true;
          else if (obj.portalType === 'mirror_off') next.mirror = false;
          else if (obj.portalType === 'dual_on') next.dual = true;
          else if (obj.portalType === 'dual_off') next.dual = false;
          else if (obj.portalType === 'vehicle_cube') next.vehicle = VehicleType.Cube;
          else if (obj.portalType === 'vehicle_ship') next.vehicle = VehicleType.Ship;
          else if (obj.portalType === 'vehicle_ball') next.vehicle = VehicleType.Ball;
          else if (obj.portalType === 'vehicle_ufo') next.vehicle = VehicleType.Ufo;
          else if (obj.portalType === 'vehicle_wave') next.vehicle = VehicleType.Wave;
          else if (obj.portalType === 'vehicle_robot') next.vehicle = VehicleType.Robot;
          else if (obj.portalType === 'vehicle_spider') next.vehicle = VehicleType.Spider;
        } else if (obj.category === 'pad') {
          if (obj.padType === PadType.Yellow) {
            next.vy = (next.small ? 691.2 : 864.0) * PHYS_DT * grav;
            next.grounded = false;
          } else if (obj.padType === PadType.Pink) {
            next.vy = (next.small ? 449.28 : 561.6) * PHYS_DT * grav;
            next.grounded = false;
          } else if (obj.padType === PadType.Red) {
            next.vy = (next.small ? 864.0 : 1080.0) * PHYS_DT * grav;
            next.grounded = false;
          } else if (obj.padType === PadType.Blue) {
            next.upsideDown = !next.upsideDown;
            next.vy = (next.small ? 276.48 : 345.6) * PHYS_DT * (next.upsideDown ? -1 : 1);
            next.grounded = false;
          } else if (obj.padType === PadType.Spider) {
            this.executeSpiderTeleport(next, objects);
          }
        } else if (obj.category === 'orb' && justPressed) {
          if (obj.orbType === OrbType.Yellow) {
            next.vy = (next.small ? 482.976 : 603.72) * PHYS_DT * grav;
            next.grounded = false;
          } else if (obj.orbType === OrbType.Pink) {
            next.vy = (next.small ? 347.76 : 434.7) * PHYS_DT * grav;
            next.grounded = false;
          } else if (obj.orbType === OrbType.Red) {
            next.vy = (next.small ? 654.858 : 821.448) * PHYS_DT * grav;
            next.grounded = false;
          } else if (obj.orbType === OrbType.Blue) {
            next.upsideDown = !next.upsideDown;
            next.vy = (next.small ? 193.185 : 241.488) * PHYS_DT * (next.upsideDown ? -1 : 1);
            next.grounded = false;
          } else if (obj.orbType === OrbType.Green) {
            next.upsideDown = !next.upsideDown;
            next.vy = (next.small ? 471.312 : 592.056) * PHYS_DT * (next.upsideDown ? -1 : 1);
            next.grounded = false;
          } else if (obj.orbType === OrbType.Black) {
            next.vy = -750.0 * PHYS_DT * grav;
            next.grounded = false;
          } else if (obj.orbType === OrbType.Spider) {
            this.executeSpiderTeleport(next, objects);
          } else if (obj.orbType === OrbType.Dash || obj.orbType === OrbType.GravityDash) {
            next.isDashing = true;
            next.dashAngle = obj.rotation || 0;
            if (obj.orbType === OrbType.GravityDash) {
              next.upsideDown = !next.upsideDown;
            }
            next.vy = 0;
            next.grounded = false;
          }
        }
      }
    }

    // 3. Vehicle-specific Movement Physics
    if (!next.isDashing) {
      switch (next.vehicle) {
        case VehicleType.Cube: {
          const gravityAcc = -2793.4752 * PHYS_DT * PHYS_DT * grav;
          if (next.grounded) {
            next.coyoteFrames = 0;
            if (effectiveInput) {
              const jumpVel = (next.small ? 482.976 : 603.72) * PHYS_DT * grav;
              next.vy = jumpVel;
              next.grounded = false;
            } else {
              next.vy = 0;
            }
          } else {
            next.coyoteFrames++;
            next.vy += gravityAcc;
            next.rotation += 8.5 * grav;
          }
          break;
        }

        case VehicleType.Ship: {
          const shipGrav = (next.small ? 1200.0 : 950.0) * PHYS_DT * PHYS_DT * grav;
          const thrust = (next.small ? 1500.0 : 1250.0) * PHYS_DT * PHYS_DT * grav;
          if (effectiveInput) {
            next.vy += thrust;
          } else {
            next.vy -= shipGrav;
          }
          // Clamp terminal velocity
          const maxShipV = 600 * PHYS_DT;
          next.vy = Math.max(-maxShipV, Math.min(maxShipV, next.vy));
          next.rotation = (next.vy / (maxShipV)) * 30 * grav;
          next.grounded = false;
          break;
        }

        case VehicleType.Ball: {
          const ballGrav = -2793.4752 * PHYS_DT * PHYS_DT * grav;
          if (next.grounded) {
            if (effectiveInput && !current.input) {
              next.upsideDown = !next.upsideDown;
              next.grounded = false;
              next.vy = -next.vy;
            } else {
              next.vy = 0;
            }
          } else {
            next.vy += ballGrav;
            next.rotation += 12 * (next.upsideDown ? -1 : 1);
          }
          break;
        }

        case VehicleType.Ufo: {
          const ufoGrav = -2100.0 * PHYS_DT * PHYS_DT * grav;
          if (justPressed) {
            next.vy = (next.small ? 420.0 : 490.0) * PHYS_DT * grav;
            next.grounded = false;
          } else {
            next.vy += ufoGrav;
          }
          break;
        }

        case VehicleType.Wave: {
          const waveSpeedY = (next.small ? 450.0 : 380.0) * PHYS_DT;
          if (effectiveInput) {
            next.vy = waveSpeedY * grav;
            next.rotation = 45 * grav;
          } else {
            next.vy = -waveSpeedY * grav;
            next.rotation = -45 * grav;
          }
          next.grounded = false;
          break;
        }

        case VehicleType.Robot: {
          const robotGrav = -2793.4752 * PHYS_DT * PHYS_DT * grav;
          if (next.grounded) {
            next.robotBoostFrames = 0;
            if (effectiveInput) {
              next.vy = (next.small ? 400.0 : 500.0) * PHYS_DT * grav;
              next.grounded = false;
              next.robotBoostFrames = 1;
            } else {
              next.vy = 0;
            }
          } else {
            if (effectiveInput && next.robotBoostFrames > 0 && next.robotBoostFrames < 35) {
              next.robotBoostFrames++;
              next.vy += (180.0 * PHYS_DT * PHYS_DT * grav);
            } else {
              next.robotBoostFrames = 0;
            }
            next.vy += robotGrav;
          }
          break;
        }

        case VehicleType.Spider: {
          const spiderGrav = -2793.4752 * PHYS_DT * PHYS_DT * grav;
          if (next.grounded) {
            if (effectiveInput && !current.input) {
              this.executeSpiderTeleport(next, objects);
            } else {
              next.vy = 0;
            }
          } else {
            next.vy += spiderGrav;
          }
          break;
        }
      }
    }

    // 4. Update Position
    next.x += next.vx;
    next.y += next.vy;

    // Floor and Ceiling bounds
    const floorY = 15;
    const ceilingY = 320;

    if (next.y <= floorY) {
      next.y = floorY;
      if (!next.upsideDown) {
        next.grounded = true;
        next.vy = 0;
        if (next.vehicle === VehicleType.Cube || next.vehicle === VehicleType.Robot) {
          next.rotation = Math.round(next.rotation / 90) * 90;
        }
      } else {
        // Hitting floor while upside down is ceiling death unless H-Block
        if (!next.hasHBlock && (next.vehicle === VehicleType.Cube || next.vehicle === VehicleType.Robot || next.vehicle === VehicleType.Wave)) {
          next.dead = true;
        }
      }
    }

    if (next.y >= ceilingY) {
      next.y = ceilingY;
      if (next.upsideDown) {
        next.grounded = true;
        next.vy = 0;
      } else {
        if (!next.hasHBlock && (next.vehicle === VehicleType.Cube || next.vehicle === VehicleType.Robot || next.vehicle === VehicleType.Wave)) {
          next.dead = true;
        }
      }
    }

    // 5. Solid Blocks & Hazards Collisions
    for (const obj of nearby) {
      if (obj.category === 'spike' || obj.category === 'saw') {
        const spikeR = obj.category === 'saw' ? obj.width * 0.45 : obj.width * 0.35;
        const dx = next.x - obj.x;
        const dy = next.y - obj.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < spikeR + size.width * 0.35) {
          next.dead = true;
          break;
        }
      } else if (obj.category === 'block') {
        const halfW = obj.width / 2;
        const halfH = obj.height / 2;
        const pLeft = next.x - size.width / 2;
        const pRight = next.x + size.width / 2;
        const pBottom = next.y - size.height / 2;
        const pTop = next.y + size.height / 2;

        const bLeft = obj.x - halfW;
        const bRight = obj.x + halfW;
        const bBottom = obj.y - halfH;
        const bTop = obj.y + halfH;

        const overlapX = Math.min(pRight, bRight) - Math.max(pLeft, bLeft);
        const overlapY = Math.min(pTop, bTop) - Math.max(pBottom, bBottom);

        if (overlapX > 2 && overlapY > 2) {
          if (next.vehicle === VehicleType.Wave) {
            if (next.hasDBlock) {
              // D-block allows sliding
              if (next.vy < 0) {
                next.y = bTop + size.height / 2;
                next.vy = 0;
              } else {
                next.y = bBottom - size.height / 2;
                next.vy = 0;
              }
            } else {
              next.dead = true;
            }
          } else {
            // Check if landing on top
            const landingTop = !next.upsideDown && next.vy <= 0 && (pBottom >= bTop - 8 || current.y - size.height / 2 >= bTop - 4);
            const landingBottom = next.upsideDown && next.vy >= 0 && (pTop <= bBottom + 8 || current.y + size.height / 2 <= bBottom + 4);

            if (landingTop) {
              next.y = bTop + size.height / 2;
              next.vy = 0;
              next.grounded = true;
              if (next.vehicle === VehicleType.Cube || next.vehicle === VehicleType.Robot) {
                next.rotation = Math.round(next.rotation / 90) * 90;
              }
            } else if (landingBottom) {
              next.y = bBottom - size.height / 2;
              next.vy = 0;
              next.grounded = true;
            } else {
              // Head on collision or ceiling bump
              const headBump = (!next.upsideDown && pTop <= bBottom + 6) || (next.upsideDown && pBottom >= bTop - 6);
              if (headBump && (next.hasHBlock || next.vehicle === VehicleType.Ship || next.vehicle === VehicleType.Ufo || next.vehicle === VehicleType.Ball)) {
                if (!next.upsideDown) next.y = bBottom - size.height / 2;
                else next.y = bTop + size.height / 2;
                next.vy = 0;
              } else {
                next.dead = true;
              }
            }
          }
        }
      }
    }

    return next;
  }

  public static executeSpiderTeleport(player: PlayerState, objects: LevelObject[]) {
    const size = this.getPlayerSize(player);
    player.upsideDown = !player.upsideDown;
    const playerLeft = player.x - size.width / 2;
    const playerRight = player.x + size.width / 2;
    const currentY = player.y;

    let targetY = player.upsideDown ? 300 : 15;
    let closestDist = 999999;
    let hitHazard = false;

    for (const obj of objects) {
      const objLeft = obj.x - obj.width / 2;
      const objRight = obj.x + obj.width / 2;

      if (playerRight >= objLeft && playerLeft <= objRight) {
        if (player.upsideDown) {
          // Teleport upwards towards ceiling
          const surfY = obj.y - obj.height / 2;
          if (surfY > currentY - 1) {
            const dist = surfY - currentY;
            if (dist < closestDist) {
              if (obj.category === 'block') {
                closestDist = dist;
                targetY = surfY - size.height / 2;
                hitHazard = false;
              } else if (obj.category === 'spike' || obj.category === 'saw') {
                closestDist = dist;
                targetY = surfY;
                hitHazard = true;
              }
            }
          }
        } else {
          // Teleport downwards towards floor
          const surfY = obj.y + obj.height / 2;
          if (surfY < currentY + 1) {
            const dist = currentY - surfY;
            if (dist < closestDist) {
              if (obj.category === 'block') {
                closestDist = dist;
                targetY = surfY + size.height / 2;
                hitHazard = false;
              } else if (obj.category === 'spike' || obj.category === 'saw') {
                closestDist = dist;
                targetY = surfY;
                hitHazard = true;
              }
            }
          }
        }
      }
    }

    player.lastSpiderRay = { fromY: currentY, toY: targetY, hitHazard };

    if (hitHazard) {
      player.y = targetY;
      player.dead = true;
    } else {
      player.y = targetY;
      player.vy = 0;
      player.grounded = true;
    }
  }
}
