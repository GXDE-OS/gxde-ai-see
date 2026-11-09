const CONTEXT = 3
// Past this many cells the middle shows as one replaced block instead.
const MAX_CELLS = 4_000_000

function lines(text: string): string[] {
  if (text === '') return []
  const all = text.split('\n')
  if (all.at(-1) === '') all.pop()
  return all
}

/** Line operations for the differing middle, deletions before additions. */
function middle(a: string[], b: string[]): string[] {
  if (a.length * b.length > MAX_CELLS) return [...a.map(line => `-${line}`), ...b.map(line => `+${line}`)]
  const width = b.length + 1
  const common = new Uint32Array((a.length + 1) * width)
  for (let i = a.length - 1; i >= 0; i--) {
    for (let j = b.length - 1; j >= 0; j--) {
      common[i * width + j] = a[i] === b[j]
        ? common[(i + 1) * width + j + 1]! + 1
        : Math.max(common[(i + 1) * width + j]!, common[i * width + j + 1]!)
    }
  }
  const ops: string[] = []
  let i = 0
  let j = 0
  while (i < a.length || j < b.length) {
    if (i < a.length && j < b.length && a[i] === b[j]) {
      ops.push(` ${a[i++]}`)
      j++
    }
    else if (j === b.length || (i < a.length && common[(i + 1) * width + j]! >= common[i * width + j + 1]!)) {
      ops.push(`-${a[i++]}`)
    }
    else {
      ops.push(`+${b[j++]}`)
    }
  }
  return ops
}

/** Unified diff hunks: lines prefixed by ' ', '-' or '+', each hunk led by its @@ header. */
export function unifiedDiff(before: string, after: string): string[] {
  const a = lines(before)
  const b = lines(after)
  let head = 0
  while (head < a.length && head < b.length && a[head] === b[head]) head++
  let tail = 0
  while (tail < a.length - head && tail < b.length - head && a[a.length - 1 - tail] === b[b.length - 1 - tail]) tail++
  const ops = [
    ...a.slice(0, head).map(line => ` ${line}`),
    ...middle(a.slice(head, a.length - tail), b.slice(head, b.length - tail)),
    ...a.slice(a.length - tail).map(line => ` ${line}`),
  ]

  const out: string[] = []
  let at = 0
  let oldLine = 1
  let newLine = 1
  while (at < ops.length) {
    const change = ops.findIndex((op, index) => index >= at && op[0] !== ' ')
    if (change === -1) break
    const start = Math.max(at, change - CONTEXT)
    for (let skip = at; skip < start; skip++) {
      oldLine++
      newLine++
    }
    let end = change
    let quiet = 0
    while (end < ops.length && quiet <= 2 * CONTEXT) {
      quiet = ops[end]![0] === ' ' ? quiet + 1 : 0
      end++
    }
    end -= Math.max(0, quiet - CONTEXT)
    const hunk = ops.slice(start, end)
    const removed = hunk.filter(op => op[0] !== '+').length
    const added = hunk.filter(op => op[0] !== '-').length
    out.push(`@@ -${removed === 0 ? oldLine - 1 : oldLine},${removed} +${added === 0 ? newLine - 1 : newLine},${added} @@`, ...hunk)
    oldLine += removed
    newLine += added
    at = end
  }
  return out
}
