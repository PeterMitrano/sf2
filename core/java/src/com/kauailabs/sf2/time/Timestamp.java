package com.kauailabs.sf2.time;

import com.kauailabs.sf2.quantity.Count;
import com.kauailabs.sf2.quantity.IQuantity;
import com.kauailabs.sf2.quantity.IQuantityContainer;

public class Timestamp implements IQuantity, IQuantityContainer {

	public enum TimestampResolution { Second, Millisecond, Microsecond, Nanosecond };
	long timestamp;
	TimestampResolution resolution;
	
	public Timestamp(Timestamp ts_copy) {
		this.timestamp = ts_copy.timestamp;
		this.resolution = ts_copy.resolution;
	}
	
	public Timestamp(long timestamp, TimestampResolution resolution) {
		this.timestamp = timestamp;
		this.resolution = resolution;
	}
	
	public Timestamp(double seconds, TimestampResolution resolution) {
		this.resolution = resolution;
		this.fromSeconds(seconds);
	}
	
	public long getTimestamp() {
		return this.timestamp;
	}
	
	public void setTimestamp(long new_timestamp) {
		this.timestamp = new_timestamp;
	}
	
	public static final long MILLISECONDS_PER_SECOND = 1000;
	public static final long MICROSECONDS_PER_SECOND = MILLISECONDS_PER_SECOND * 1000;
	public static final long NANOSECONDS_PER_SECOND = MICROSECONDS_PER_SECOND * 1000;	
	public static final long NANOSECONDS_PER_MICROSECOND = 1000;
	public static final long MICROSECONDS_PER_MILLISECOND = 1000;
	public static final long NANOSECONDS_PER_MILLISECOND = NANOSECONDS_PER_MICROSECOND * 1000;	
	
	public long getNanoseconds() {
		switch ( resolution ) {		
		case Second:
			return timestamp * NANOSECONDS_PER_SECOND;
		case Millisecond:
			return timestamp * NANOSECONDS_PER_MILLISECOND;
		case Microsecond:
			return timestamp * NANOSECONDS_PER_MICROSECOND;
		case Nanosecond:
		default:
			return timestamp;
		}
	}
	
	public long getMicroseconds() {
		switch ( resolution ) {		
		case Second:
			return timestamp * MICROSECONDS_PER_SECOND;
		case Millisecond:
			return timestamp * MICROSECONDS_PER_MILLISECOND;
		case Microsecond:
		default:
			return timestamp;
		case Nanosecond:
			return timestamp / NANOSECONDS_PER_MICROSECOND;
		}
	}
	
	public long getMilliseconds() {
		switch ( resolution ) {		
		case Second:
			return timestamp * MILLISECONDS_PER_SECOND;
		case Millisecond:
		default:
			return timestamp;
		case Microsecond:
			return timestamp / MICROSECONDS_PER_MILLISECOND;
		case Nanosecond:
			return timestamp / NANOSECONDS_PER_MILLISECOND;
		}
	}
	
	public double getSeconds() {
		switch ( resolution ) {		
		case Second:
			return (double)timestamp;
		case Millisecond:
		default:
			return ((double)timestamp)/MILLISECONDS_PER_SECOND;
		case Microsecond:
			return ((double)timestamp)/MICROSECONDS_PER_SECOND;
		case Nanosecond:
			return ((double)timestamp)/NANOSECONDS_PER_SECOND;
		}
	}
	
	public void fromSeconds(double seconds) {
		switch ( resolution ) {		
		case Second:
			timestamp = (long)seconds;
		case Millisecond:
		default:
			timestamp = (long)(seconds * MILLISECONDS_PER_SECOND);
		case Microsecond:
			timestamp = (long)(seconds * MICROSECONDS_PER_SECOND);
		case Nanosecond:
			timestamp = (long)(seconds * NANOSECONDS_PER_SECOND);
		}	
	}

	@Override
	public void getQuantities(IQuantity[] quantities) {
		quantities = new IQuantity[]{
			new Count(timestamp)
		};
	}

	@Override
	public void getPrintableString(String printable_string) {
		printable_string = Long.toString(timestamp);
	}
}