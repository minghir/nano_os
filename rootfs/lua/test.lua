-- Joc de ghicit numere în Lua (Nano OS Edition)
print("========================================")
print("  JOC: Ghiceste numarul (1 - 50)        ")
print("========================================")

math.randomseed(12345) -- Seed fix sau bazat pe timp dacă e disponibil
local secret = math.random(1, 50)
local attempts = 0
local guess = 0

repeat
    io.write("Introdu numarul tau: ")
    io.flush()
    
    local input = io.read()
    if not input then break end
    
    guess = tonumber(input)
    
    if not guess then
        print("Te rog introdu un numar valid!")
    else
    attempts = attempts + 1
        if guess < secret then
            print("Prea mic! Incearca un numar mai mare.")
        elseif guess > secret then
            print("Prea mare! Incearca un numar mai mic.")
        else
            print(string.format("\nBravo! Ai ghicit numarul %d din %d incercari! 🚀", secret, attempts))
        end
    end
until guess == secret