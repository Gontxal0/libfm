//      job.vala
//      
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//      
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//      
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//      
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.
//      
//      

namespace Fm {

// defines severity of errors
public enum Severity {
	NONE,
	WARNING, // not an error, just a warning
	MILD, // no big deal, can be ignored most of the time
	MODERATE, // moderate errors
	SEVERE, // severe errors, whether to abort operation depends on error handlers
	CRITICAL // critical errors, the operation is aborted
}

// actions to take once errors happen
public enum ErrorAction {
	CONTINUE, // ignore the error and continue remaining work
	RETRY, // retry the previously failed operation. (not every kind of job support this) */
	ABORT // abort the whole job
}

public abstract class Job2 : Object {

	public Job2() {
		this.cancellable = new Cancellable();
	}

	~Job2() {
		cancellable.disconnect(cancelled_handler);
	}

	public bool run_sync() {
		running = true;
		var ret = run();
		finished();
		return ret;
	}

	private bool job_func(IOSchedulerJob job, Cancellable? cancellable) {
		this.job = job;
		run();
		this.job = null;
		job.send_to_mainloop(() => {
			finished();
			return true;
		});
		return false;
	}

	public virtual void run_async() {
		running = true;
		g_io_scheduler_push_job(job_func, Priority.DEFAULT, cancellable);
	}

	// call from the "worker thread" to do the real work
	// should be overriden by derived classes
	protected abstract bool run();

	public void cancel() {
		cancellable.cancel();
	}

	public bool is_cancelled() {
		return cancellable.is_cancelled();
	}

	public bool is_running() {
		return running;
	}

	protected unowned Cancellable get_cancellable() {
		return cancellable;
	}

	// The "finished" signal is emitted when the job is cancelled as well.
	// Just check if the job is cancelled with is_cancelled().
	public virtual signal void finished() {
	}

	protected unowned IOSchedulerJob? job;
	protected Cancellable cancellable;
	protected bool running;
	private ulong cancelled_handler;
}

}
