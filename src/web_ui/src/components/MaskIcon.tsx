import local from './MaskIcon.module.css'

// Draw SVG in current color.
export function MaskIcon({ src, size }: { src: string; size: number }) {
  const image = `url("${src}")`
  return <span className={local.icon} aria-hidden="true" style={{ width: size, height: size, maskImage: image, WebkitMaskImage: image }} />
}
