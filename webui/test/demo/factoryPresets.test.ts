// This plugin's factory preset files and its analyser switch in the demo. The rest is tested in SampleRealm-Modules
import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test } from 'node:test';
import { parsePreset } from '@samplerealm/modules/demo/localPresets.ts';
import { parseState } from '@samplerealm/modules/demo/workletBackend.ts';
import { memoryStorage } from '@samplerealm/modules/testing/presets.ts';
import { workletBackend } from '../../src/demo/workletBackend.ts';
import { info } from '../engine.ts';

const presetFolder = resolve(import.meta.dirname, '../../../Assets/Presets');
const factoryPresets = readdirSync(presetFolder)
  .filter((file) => file.endsWith('.farside'))
  .map((file) => readFileSync(resolve(presetFolder, file), 'utf8'));

const backendForTest = () => workletBackend(info, { factoryPresets, storage: memoryStorage() });

// As Tests/PresetTests.cpp
test('every shipped factory preset sets every parameter but the analyser switch in range, and loads unmodified', async () => {
  assert.notEqual(factoryPresets.length, 0);
  const { presets, getValue } = backendForTest();

  for (const xml of factoryPresets) {
    const { name, category, values } = parsePreset(xml, parseState);

    await presets.load({ name, category, isFactory: true });
    assert.deepEqual(presets.status(), { current: { name, category, isFactory: true }, modified: false });

    for (const { id, min, max, isPerformanceControl } of info.params) {
      const value = values[id];

      if (isPerformanceControl) {
        assert.equal(value, undefined, `${name}: ${id}`);
        continue;
      }

      assert.ok(value !== undefined && value >= min && value <= max, `${name}: ${id}`);
      assert.equal(getValue(id), value, `${name}: ${id}`);
    }
  }
});

// As Tests/PresetTests.cpp
test("the analyser switch doesn't mark the preset modified, but a knob does", async () => {
  const { presets, setValue } = backendForTest();
  await presets.load({ name: 'Vocal Plate', category: 'Lush', isFactory: true });

  setValue('spectrumBypass', 1);
  assert.equal(presets.status().modified, false);

  setValue('mix', 0.9);
  assert.equal(presets.status().modified, true);
});
