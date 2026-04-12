import sys
import re

if len(sys.argv) < 2:
    print("Usage: python compiler.py <source_file>")
    sys.exit(1)

filename = sys.argv[1]

with open(filename, 'r') as f:
    lines = f.readlines()
    
cleaned = []
for line in lines:
    line = line.split("#")[0].strip()
    if line:
        cleaned.append(line)
        
mem_data = []
mem_labels = {}
remaining = []

for line in cleaned:
    if line.startswith("."):
        match = re.match(r'\.(\w+):\s*(.*)', line)
        if match:
            label = match.group(1)
            value = list(map(int, match.group(2).split()))
            mem_labels[label] = len(mem_data)
            mem_data.extend(value)
    else:
        remaining.append(line)
        
branch_label = {}
instruction = []
pc = 0

for line in remaining:
    match = re.match(r'^(\w+):\s*$', line)
    if match:
        branch_label[match.group(1)] = pc
        continue
    
    match = re.match(r'^(\w+):\s+(.+)$', line)
    if match:
        branch_label[match.group(1)] = pc
        line = match.group(2).strip()
        
    instruction.append(line)
    pc += 1
    
output = []
for inst in instruction:
    for label, addr in mem_labels.items():
        inst = re.sub(r'\b' + re.escape(label) + r'\b', str(addr), inst)
    
    for label, addr in branch_label.items():
        inst = re.sub(r'\b' + re.escape(label) + r'\b', str(addr), inst)
        
    parts = inst.split(',')
    if len(parts) >= 3:
        last = parts[-1].strip()
        if last in branch_label:
            parts[-1] = ' ' + str(branch_label[last])
            inst = ','.join(parts)
            
    tokens = inst.split()
    if tokens[0] == 'j' and len(tokens) == 2 and tokens[1] in branch_label:
        inst = 'j ' + str(branch_label[tokens[1]])
    
    output.append(inst)
    
with open(filename, 'w') as f:
    # CRITICAL FIX: Added '#' so Processor.h can parse it correctly
    if mem_data:
        f.write('#MEM ' + ' '.join(map(str, mem_data)) + '\n')
    else:
        f.write('#MEM\n')
        
    for inst in output:
        f.write(inst + '\n')