require 'socket'
require 'json'

puts ARGV.inspect
host=ARGV[0] || (raise "No $0 host")
port=ARGV[1] || (raise "No $1 port")
input_list = ARGV[2..-1]
(raise "No $1 port") if ARGV.size <=2

puts "Opening socket to #{host}:#{port}, input files #{input_list.to_json}"

send_ct = 0
Socket.tcp(host, port.to_i) do |sock|
  puts "socket opened"
  hex_f = input_list[send_ct]
  break if hex_f.nil? # finished

  puts "Read #{hex_f}"
  s = File.read(hex_f)
  bytes = []
  s.split("\n").map { |l|
    next if l.strip.start_with?('#') # comment line
    hex_segs = l.strip.split(" ").select { |seg| seg =~ /^[0-9A-Za-z]{2}$/ }
    puts("\t" + hex_segs.join(' '))
    hex_bytes = hex_segs.map { |ab| ab.to_i(16) }
    bytes += hex_bytes
  }
  puts "Read #{hex_f} #{bytes.size} bytes got"

  send_bytes = bytes
  puts "--> sending #{send_bytes.size} bytes"
  send_bytes.each { |i|
    sock.send(i.chr, 0) # Send the byte 0xff.
  }
  puts "<-- waiting recv bytes:"
  resp = sock.recv(10*1024) # Read a byte from the remote host.
  recv_ct = 0
  resp.split('').each { |c|
    recv_ct += 1
    print "#{c.ord.to_s(16).ljust(2, '0')} "
    print "\n" if (recv_ct+1) % 16 == 0
  }
  puts "\n #{recv_ct} bytes"
end
