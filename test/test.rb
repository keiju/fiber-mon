
require "fiber-mon"

Thread.abort_on_exception = true

mon = FiberMon.new

case ARGV[0]
when "0"
  mon.entry{p 1}
  sleep 5
when "1"
#  Thread.abort_on_exception = true
  mon.entry{p 1; raise "aaa"}
  sleep 5

when "2"
  mon.entry{loop{p 1; mon.yield}}
  mon.entry{loop{p 2; mon.yield}}
  sleep 1

when "3"
  mx = mon.new_mon
  mon.entry{loop{mx.synchronize do p 1; end; mon.yield}}
  mon.entry{loop{mx.synchronize do p 2; end; mon.yield}}
  Thread.start do
    loop do
      mx.synchronize{p 3}
    end
  end
      
  sleep 1

when "4"
  mx = mon.new_mon
  cv = mx.new_cv
  x = -1
  mon.entry do
    c = 0
    loop do 
      mx.synchronize do 
	cv.wait_until{x == 0}
	puts "A: #{c += 1}"
	x = -1
      end
      mon.yield
    end
  end
  mon.entry do
    c = 0
    loop do 
      mx.synchronize do 
	cv.wait_until{x == 1}
	puts "B: #{c += 1}"
	x = -1
      end
      mon.yield
    end
  end


  Thread.start do
    loop do
      mx.synchronize do
	puts "C"
	x = rand(2)
	cv.broadcast
      end
    end
  end
      
  sleep 2
end

  
