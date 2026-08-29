import { LevelObject, PlayerState, SolverResult } from '../types/gd';
import { GDPhysicsEngine } from './physics';

export class LevelSolver {
  public static solve(objects: LevelObject[], targetDistance: number = 1000, maxSteps: number = 1800): SolverResult {
    const startTime = performance.now();
    let exploredNodes = 0;

    // We search through state checkpoints at decision points
    interface SearchNode {
      state: PlayerState;
      inputs: boolean[];
      trajectory: PlayerState[];
    }

    const initial = GDPhysicsEngine.createInitialState();
    const queue: SearchNode[] = [{
      state: initial,
      inputs: [],
      trajectory: [initial]
    }];

    let bestNode: SearchNode = queue[0];
    let maxX = 0;

    const maxQueue = 1200;

    while (queue.length > 0 && exploredNodes < maxSteps) {
      // Pick node with best X progress
      queue.sort((a, b) => b.state.x - a.state.x);
      const current = queue.shift()!;
      exploredNodes++;

      if (current.state.x > maxX) {
        maxX = current.state.x;
        bestNode = current;
      }

      // Check if target reached
      if (current.state.x >= targetDistance) {
        const timeMs = Math.round(performance.now() - startTime);
        return {
          solved: true,
          totalFrames: current.inputs.length,
          exploredNodes,
          timeMs,
          inputs: current.inputs,
          trajectory: current.trajectory,
          macroEvents: this.inputsToMacro(current.inputs)
        };
      }

      // Try candidates:
      // 1. Release / Hold nothing for N frames (e.g. 3 to 12 frames)
      // 2. Click / Tap for 1-4 frames
      // 3. Hold for 10-30 frames (for ship / wave / dash / robot boost)
      const actions: { inputPattern: boolean[] }[] = [
        { inputPattern: [false, false, false, false] },
        { inputPattern: [true, false, false, false] },
        { inputPattern: [true, true, true, false] },
        { inputPattern: [true, true, true, true, true, true, false] },
        { inputPattern: [true, true, true, true, true, true, true, true, true, true, true, true] },
        { inputPattern: [false, false, false, false, false, false, false, false] },
      ];

      for (const action of actions) {
        let simState = current.state;
        const simTraj: PlayerState[] = [...current.trajectory];
        const simInputs: boolean[] = [...current.inputs];
        let died = false;

        for (const inp of action.inputPattern) {
          simState = GDPhysicsEngine.step(simState, inp, objects);
          simTraj.push(simState);
          simInputs.push(inp);

          if (simState.dead) {
            died = true;
            break;
          }
        }

        if (!died && queue.length < maxQueue) {
          queue.push({
            state: simState,
            inputs: simInputs,
            trajectory: simTraj
          });
        }
      }
    }

    // Return best attempt if not fully reached target
    const timeMs = Math.round(performance.now() - startTime);
    return {
      solved: bestNode.state.x >= targetDistance,
      totalFrames: bestNode.inputs.length,
      exploredNodes,
      timeMs,
      inputs: bestNode.inputs,
      trajectory: bestNode.trajectory,
      macroEvents: this.inputsToMacro(bestNode.inputs)
    };
  }

  public static inputsToMacro(inputs: boolean[]): { frame: number; down: boolean }[] {
    const events: { frame: number; down: boolean }[] = [];
    let isDown = false;

    for (let f = 0; f < inputs.length; f++) {
      if (inputs[f] && !isDown) {
        events.push({ frame: f, down: true });
        isDown = true;
      } else if (!inputs[f] && isDown) {
        events.push({ frame: f, down: false });
        isDown = false;
      }
    }

    return events;
  }

  public static exportFormat(events: { frame: number; down: boolean }[], format: 'json' | 'xbot' | 'megahack' | 'plain'): string {
    if (format === 'json') {
      return JSON.stringify({
        generator: 'Pathfinder v1.0.0-beta.243',
        fps: 240,
        totalActions: events.length,
        events: events.map(e => ({ frame: e.frame, hold: e.down, player: 1 }))
      }, null, 2);
    }

    if (format === 'xbot') {
      let out = `[xBot Macro 240FPS]\n`;
      for (const e of events) {
        out += `${e.frame} | ${e.down ? 'down' : 'up'} | p1\n`;
      }
      return out;
    }

    if (format === 'megahack') {
      let out = `{"fps":240,"inputs":[\n`;
      out += events.map(e => `  {"frame":${e.frame},"btn":1,"down":${e.down}}`).join(',\n');
      out += `\n]}`;
      return out;
    }

    return events.map(e => `Frame ${e.frame.toString().padStart(5, ' ')}: ${e.down ? 'CLICK [DOWN]' : 'RELEASE [UP]'}`).join('\n');
  }
}
