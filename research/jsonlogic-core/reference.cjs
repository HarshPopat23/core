// Core compatibility contrasts, not a comparison of JsonLogic engine performance.
// Pass logic.js from json-logic-js commit c5c73601c90b11e98f6846609bac4dec203d1c18.
const assert = require('node:assert/strict');
const path = require('node:path');
const logic = require(path.resolve(process.argv[2]));
const checked = [];
function check(name, test) {
  test();
  checked.push(name);
}
const variable = (key) => ({ var: key });
check('binary arithmetic', () => assert.equal(logic.apply({ '+': [0.1, 0.2] }), 0.30000000000000004));
check('large integer ingress', () => {
  const data = JSON.parse('{"a":9007199254740992,"b":9007199254740993}');
  assert.equal(logic.apply({ '===': [variable('a'), variable('b')] }, data), true);
});
check('object identity', () => assert.equal(logic.apply({ '===': [variable('a'), variable('b')] }, { a: { x: 1 }, b: { x: 1 } }), false));
check('array identity', () => assert.equal(logic.apply({ '===': [variable('a'), variable('b')] }, { a: [1], b: [1] }), false));
check('null ordering', () => assert.equal(logic.apply({ '<': [null, -5] }), false));
check('string numeric ordering', () => assert.equal(logic.apply({ '<': ['4', 5] }), true));
check('string lexical ordering', () => assert.equal(logic.apply({ '<=': ['10', '2'] }), true));
check('numeric prefix', () => assert.equal(logic.apply({ '+': ['4kg', 1] }), 5));
check('whitespace numeric conversion', () => assert.equal(logic.apply({ '<': [' 4 ', 5] }), true));
check('empty array truthiness', () => assert.equal(logic.truthy([]), false));
check('empty object truthiness', () => assert.equal(logic.truthy({}), true));
check('empty all', () => assert.equal(logic.apply({ all: [[], true] }), false));
check('current item scope', () => assert.equal(logic.apply({ all: [variable('items'), { '<=': [variable('quantity'), variable('limit')] }] }, { limit: 3, items: [{ quantity: 2 }] }), false));
check('dot path', () => assert.equal(logic.apply(variable('a.b'), { 'a.b': 1, a: { b: 2 } }), 2));
check('empty var is current data', () => assert.deepEqual(logic.apply(variable(''), { '': 3 }), { '': 3 }));
check('object syntax is not a quote', () => assert.throws(() => logic.apply({ status: 'ok' }), /Unrecognized operation status/));
check('missing conflates null and empty string', () => assert.deepEqual(logic.apply({ missing: ['absent', 'n', 'e'] }, { n: null, e: '' }), ['absent', 'n', 'e']));
check('UTF16 slicing', () => assert.equal(logic.apply({ substr: ['😀X', 0, 1] }), '\ud83d'));
check('untaken unknown branch', () => assert.equal(logic.apply({ if: [false, { unknown: [] }, true] }), true));
check('custom operations are eager', () => {
  logic.add_operation('researchChoose', (condition, value) => condition ? value : null);
  try { assert.throws(() => logic.apply({ researchChoose: [false, { unknown: [] }] }), /Unrecognized operation unknown/); }
  finally { logic.rm_operation('researchChoose'); }
});
check('negative zero ingress', () => assert.equal(Object.is(JSON.parse('-0'), -0), true));
check('duplicate members last wins', () => assert.equal(logic.apply(variable('x'), JSON.parse('{"x":1,"x":2}')), 2));
check('nonfinite intermediate', () => assert.equal(logic.apply({ '/': [1, 0] }), Infinity));
check('host prototype access', () => assert.equal(typeof logic.apply(variable('toString'), {}), 'function'));
if (process.argv.includes('--negative-control')) {
  assert.equal(logic.apply({ '+': [0.1, 0.2] }), 0.3, 'deliberately wrong binary-decimal expectation');
}
console.log(JSON.stringify({ reference: 'json-logic-js', version: '2.0.5', commit: 'c5c73601c90b11e98f6846609bac4dec203d1c18', passed: checked.length, checks: checked }, null, 2));
