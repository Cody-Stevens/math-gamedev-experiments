// Per-game recording scripts: [seconds, JS run in the game page (or null), caption HTML (or null)].
// Every number in a caption comes from that game's README / bench.js (measured).
const click = sel => `document.querySelector('${sel}').click(); 1`;
const clickText = t => `[...document.querySelectorAll('button')].find(b => b.textContent.trim().startsWith(${JSON.stringify(t)})).click(); 1`;
const setVal = (id, v) => `(e => { e.value = ${JSON.stringify(v)}; e.dispatchEvent(new Event('input', {bubbles: true})); e.dispatchEvent(new Event('change', {bubbles: true})); })(document.getElementById(${JSON.stringify(id)})); 1`;
const GH = 'github.com/openai/math';
module.exports = {
  1: { url: '1-wildfire-isle/index.html?scene=compare', crop: [140, 500], settle: 9000,
    tag: 'Wildfire Isle · <span class="o">orange = best method today</span> · <span class="n">blue = built on a new proof</span>', title: '~400× less compute, same gameplay',
    sub: 'A fire\'s edge is crinkly. Sharper graphics show more crinkles, so the fire has farther to go.',
    link: `Paper: ${GH} · Natural Occupation Measures for Critical Square-Lattice FK Interfaces`,
    setup: click('[data-speed="4"]') + ';' + click('[data-q="1"]'),
    steps: [
      [0, null, 'Tuned at Medium graphics, the fire needs <b>18 hours</b> to reach the village. Both ways agree.'],
      [6, click('[data-q="3"]'), 'Switch to <b>Ultra</b>. <span class="o">OLD way: 139 hours.</span> <span class="n">NEW way: 17 hours.</span>'],
      [12, click('[data-q="0"]'), '<b>Low</b> graphics. <span class="o">OLD: 11 hours.</span> <span class="n">NEW: 18 hours.</span> Same game on every setting.'],
      [18, click('[data-q="2"]'), 'The math says exactly how much to slow the fire on each setting, so <span class="n">every setting plays the same.</span>'],
      [25, null, null]] },
  2: { url: '2-tavern-tables/index.html?scene=one', crop: [58, 640], settle: 5000,
    tag: 'Tavern of a Thousand Tables · <span class="o">orange = best method today</span> · <span class="n">blue = built on a new proof</span>', title: '1,000×+ cheaper game AI that never loses',
    sub: 'Proven: one player can always win this board game within 21 moves, and the proof spells out how. So the proof is the AI.',
    link: `Paper: ${GH} · Snaky in 21 Maker moves`,
    setup: setVal('brk', '2') + ';' + setVal('speed', '1'),
    steps: [
      [0, null, '1 table: a normal game AI has plenty of time to think, and it wins too.'],
      [6, setVal('nslider', '4'), '16 tables share the same thinking time, so each <span class="o">normal AI</span> gets less of it.'],
      [14, setVal('nslider', '6'), '64 tables. <span class="o">Normal AI: out of time, it stalls.</span> <span class="n">Proof AI: just follows the recipe, instantly.</span>'],
      [24, null, '<span class="o">Left: not one game won yet.</span> <span class="n">Right: every table won within 21 moves.</span> (Only works for this one game.)'],
      [31, null, null]] },
  3: { url: '3-colony-ledger/index.html?scene=slowrot', crop: [112, 450], settle: 4000,
    tag: 'Colony Ledger · <span class="o">orange = best method today</span> · <span class="n">blue = built on a new proof</span>', title: '1,000×+ faster check for broken economies',
    sub: 'Proven: if every crafting recipe has a way back, even via other recipes, nothing ever runs out or piles up forever.',
    link: `Paper: ${GH} · Uniform Permanence in Weakly Reversible Mass-Action Systems`,
    steps: [
      [0, null, 'Same recipe list in every town. A 10-year simulated playtest says <span class="o">PASS</span>.'],
      [6, null, 'The new check just reads the recipe list, instantly: <span class="n">FAIL, 4 recipes have no way back.</span>'],
      [12, null, 'Decades later: <span class="o">capped towns sit stuck at their limits.</span> <span class="n">Uncapped towns run out or pile up.</span>'],
      [18, clickText('Fix: add return recipes'), 'One click adds the missing way-back recipes. The check passes: <b>safe at any crafting speed.</b>'],
      [25, null, null]] },
  4: { url: '4-dungeon-daily/index.html?scene=normal', crop: [76, 486], settle: 5000,
    tag: 'Dungeon Daily · <span class="o">orange = best method today</span> · <span class="n">blue = built on a new proof</span>', title: '5–32× faster fair dungeons (with hub rooms)',
    sub: 'Every day the corridors get reshuffled, but each room keeps the same number of doors. Is every layout equally likely?',
    link: `Paper: ${GH} · Polynomial Mixing of the Switch Chain for Every Graphical Degree Sequence`,
    steps: [
      [0, null, '<span class="o">Quick shuffle (left):</span> keeps most of the old corridors. Players learn the map.'],
      [6, null, 'Normal dungeon: the usual fair method (middle) is fastest. <b>The old way wins here.</b>'],
      [12, clickText('hubs'), 'Add big hub rooms: <span class="o">the usual method has to retry ~165,000 times per layout.</span>'],
      [18, null, '<span class="n">NEW: the proof says how much shuffling is enough.</span> Fair dungeons in milliseconds, hubs or not.'],
      [25, null, null]] },
  5: { url: '5-slime-lab/index.html?scene=ball', crop: [74, 436], settle: 7000, webgl: true,
    tag: 'Slime Lab · <span class="o">orange = best method today</span> · <span class="n">blue = built on a new proof</span>', title: '13–18× faster slime shapes, and exact',
    sub: 'Slime pulls into the shape with the least surface. Here the walls wrap around, like Pac-Man.',
    link: `Paper: ${GH} · The Isoperimetric Conjecture for the Cubic Flat Three-Torus`,
    steps: [
      [0, null, '<span class="o">OLD: simulate the slime settling into shape, step by step.</span>'],
      [6, setVal('fill', '25'), 'Proven: the best shape is always a <span class="n">ball, a tube, or a flat sheet.</span> So we just draw it.'],
      [12, setVal('fill', '45'), 'Here the <span class="o">simulation got stuck</span> in a tangle. <span class="n">NEW: exactly right, about 15× faster.</span>'],
      [18, null, 'Honest limit: add a pillar or gravity and you have to simulate again.'],
      [25, null, null]] }
};
