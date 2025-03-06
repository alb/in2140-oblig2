# Oblig 2 in IN2140
We decided to change
```c
void save_inodes(...)
```

to
```c
char *save_inodes(...)
```

to use the function recursively. The return value is used for updating the pointer to where the function should continue writing.
