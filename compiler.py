import sys
import re


# VALID OPCODES (for validation / reference)

VALID_OPCODES = {
    "add", "sub", "addi", "mul", "div", "rem",
    "lw", "sw",
    "beq", "bne", "blt", "ble", "j",
    "slt", "slti",
    "and", "or", "xor", "andi", "ori", "xori"
}

def preprocess(filename):

    # STEP 1: Read file, strip comments & blanks
    with open(filename, 'r') as f:
        raw_lines = f.readlines()

    clean_lines = []
    for line in raw_lines:
        # Remove inline comments
        line = line.split('#')[0]
        # Strip whitespace
        line = line.strip()
        if line:
            clean_lines.append(line)

    # STEP 2: First pass — collect memory labels
    # .A: 1 2 3  =>  memory_labels["A"] = 0
    memory_labels = {}   # label_name -> starting memory address
    memory_data   = []   # flat list of all values to populate Memory[]
    non_memory_lines = []

    for line in clean_lines:
        if line.startswith('.'):
            # Format: .LABEL: val1 val2 val3 ...
            match = re.match(r'\.(\w+)\s*:\s*(.*)', line)
            if not match:
                raise SyntaxError(f"Malformed memory declaration: {line}")
            label = match.group(1)
            values = list(map(int, match.group(2).split()))
            memory_labels[label] = len(memory_data)
            memory_data.extend(values)
        else:
            non_memory_lines.append(line)

    # STEP 3: Second pass — collect instruction labels
    # loop:  =>  instruction_labels["loop"] = pc
    instruction_labels = {}   # label_name -> PC value
    instruction_lines  = []   # (original_line, pc) — only real instructions

    pc = 0
    for line in non_memory_lines:
        # A line can be "label: instruction" or just "label:" or just instruction
        # Handle "label:" on its own line
        if re.match(r'^\w+\s*:\s*$', line):
            label = line.rstrip(':').strip()
            instruction_labels[label] = pc
        # Handle "label: instruction" on same line
        elif re.match(r'^\w+\s*:\s*\w+', line):
            # Check if it's actually a label prefix or a memory op like lw/sw
            # Split on first colon only if left side is a single word (label)
            parts = line.split(':', 1)
            potential_label = parts[0].strip()
            rest = parts[1].strip() if len(parts) > 1 else ''
            # If rest starts with a valid opcode, it's a label + instruction
            first_word = rest.split()[0] if rest else ''
            if first_word in VALID_OPCODES:
                instruction_labels[potential_label] = pc
                instruction_lines.append((rest, pc))
                pc += 1
            else:
                # It's just an instruction (no label), treat whole line normally
                instruction_lines.append((line, pc))
                pc += 1
        else:
            instruction_lines.append((line, pc))
            pc += 1

    # STEP 4: Third pass — resolve labels in instructions
    # Replace memory labels and branch targets with numbers
    resolved_instructions = []

    for (line, current_pc) in instruction_lines:
        tokens = line.split()
        opcode = tokens[0].lower()

        # ── Memory instructions: lw x1, A(x2)  =>  lw x1, 0(x2)
        if opcode in ("lw", "sw"):
            # Rejoin and normalize commas
            rest = ' '.join(tokens[1:]).replace(',', ' ')
            rest = ' '.join(rest.split())
            # Match: reg, LABEL(reg) or reg, offset(reg)
            mem_match = re.search(r'([A-Za-z_]\w*)\(', rest)
            if mem_match:
                label_name = mem_match.group(1)
                if label_name in memory_labels:
                    rest = rest.replace(label_name, str(memory_labels[label_name]), 1)
                elif label_name.startswith('x') or label_name[0].isdigit():
                    pass  # it's a register or number, fine
                else:
                    raise NameError(f"Undefined memory label '{label_name}' in: {line}")
            resolved_instructions.append(f"{opcode} {rest}")

        # ── Branch instructions: beq x1, x2, loop  =>  beq x1, x2, -1
        elif opcode in ("beq", "bne", "blt", "ble"):
            # Handle both "beq x1, x2, loop" and "beq x1 x2 loop"
            rest = ' '.join(tokens[1:])
            # Normalize: remove commas, split on whitespace
            parts = [p.strip() for p in rest.replace(',', ' ').split()]
            if len(parts) != 3:
                raise SyntaxError(f"Bad branch instruction: {line}")
            reg1, reg2, target = parts
            if target in instruction_labels:
                # Relative offset = target_pc - current_pc
                offset = instruction_labels[target] - current_pc
                target = str(offset)
            # else assume it's already a number
            resolved_instructions.append(f"{opcode} {reg1}, {reg2}, {target}")

        # ── Jump instruction: j label  =>  j offset
        elif opcode == "j":
            if len(tokens) < 2:
                raise SyntaxError(f"Bad jump instruction: {line}")
            target = tokens[1]
            if target in instruction_labels:
                offset = instruction_labels[target] - current_pc
                target = str(offset)
            resolved_instructions.append(f"j {target}")

        # ── All other instructions: pass through as-is
        else:
            resolved_instructions.append(line)

    # STEP 5: Write output back to same file
    # Format:
    #   Line 1..N  = resolved instructions (one per line)
    # Memory data is embedded as a comment header so
    # loadProgram() can reconstruct Memory[]
    with open(filename, 'w') as f:
        # Write memory data as a special header line
        # Format: #MEM val0 val1 val2 ...
        # Your loadProgram() should look for this line first
        if memory_data:
            f.write("#MEM " + " ".join(map(str, memory_data)) + "\n")

        # Write resolved instructions
        for inst in resolved_instructions:
            f.write(inst + "\n")

    print(f"[compiler] Preprocessing complete.")
    print(f"  Instructions : {len(resolved_instructions)}")
    print(f"  Memory words : {len(memory_data)}")
    print(f"  Mem labels   : {list(memory_labels.keys())}")
    print(f"  Inst labels  : {list(instruction_labels.keys())}")


# ENTRY POINT
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 compiler.py <filename.s>")
        sys.exit(1)
    preprocess(sys.argv[1])