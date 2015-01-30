.float field;

.float getfield() {
  return field;
}

void() main = {
  entity e = spawn();
  e.field = 42;
  print(ftos(e.(getfield())), "\n");
  .float memptr = getfield();
  print(ftos(e.memptr), "\n");
}
