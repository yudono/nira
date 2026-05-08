# Object Composition and Method-like functions test

create_point x, y:
  return { x: x, y: y }

create_size w, h:
  return { w: w, h: h }

create_rect x, y, w, h:
  return {
    origin: create_point(x, y),
    size: create_size(w, h)
  }

# "Method" for area
get_area rect:
  return rect.size.w * rect.size.h

# "Method" for move
move_rect rect, dx, dy:
  rect.origin.x = rect.origin.x + dx
  rect.origin.y = rect.origin.y + dy

# "Method" for check containment
contains_point rect, p:
  if p.x < rect.origin.x:
    return false
  if p.x > rect.origin.x + rect.size.w:
    return false
  if p.y < rect.origin.y:
    return false
  if p.y > rect.origin.y + rect.size.h:
    return false
  return true

main:
  print("=== OBJECT COMPOSITION TEST ===")
  
  r1 = create_rect(10, 10, 50, 50)
  print("Rect 1 Area: " + toString(get_area(r1)))
  
  p1 = create_point(20, 20)
  p2 = create_point(5, 5)
  
  print("Contains P1 (20,20): " + toString(contains_point(r1, p1)))
  print("Contains P2 (5,5): " + toString(contains_point(r1, p2)))
  
  print("Moving rect by (20, 20)...")
  move_rect(r1, 20, 20)
  print("New Origin: (" + toString(r1.origin.x) + ", " + toString(r1.origin.y) + ")")
  
  print("Contains P1 (20,20) after move: " + toString(contains_point(r1, p1)))
  
  # Deep composition
  scene = {
    name: "Main Scene",
    shapes: [r1, create_rect(0, 0, 100, 100)],
    metadata: {
      version: 1.0,
      tags: ["vector", "graphics"]
    }
  }
  
  print("Scene: " + toString(scene.name))
  print("Shapes count: " + toString(len(scene.shapes)))
  print("First shape area: " + toString(get_area(scene.shapes[0])))
  print("Metadata Version: " + toString(scene.metadata.version))
  
  print("=== COMPOSITION TEST PASSED ===")
