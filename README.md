# cs350-os161
CS350 - Operating Systems source control

## Configuration docs:
https://www.student.cs.uwaterloo.ca/~cs350/common/WorkingWith161.html

## Reconfiguration:
```
cd $HOME/cs350-os161/cs350-os161/os161-1.99/ ./configure --ostree=$HOME/cs350-os161/cs350-os161/root --toolprefix=cs350-`
```

## Recompilation:
```
cd $HOME/cs350-os161/cs350-os161/os161-1.99/kern/conf ./config ASST0
cd ../compile/ASST0
bmake depend
bmake
bmake install
```
```
cd $HOME/cs350-os161/cs350-os161/os161-1.99/
bmake
bmake install
```

## Replace SYS Root:
```
cd $HOME/cs350-os161/cs350-os161/root
cp /u/cs350/sys161/sys161.conf sys161.conf 
```

## Run:
```
cd $HOME/cs350-os161/cs350-os161/root
sys161 kernel-ASSTX
```

